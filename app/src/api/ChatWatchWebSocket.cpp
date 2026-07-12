#include "api/ChatWatchWebSocket.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QUrlQuery>

Q_LOGGING_CATEGORY(lcChatWatch, "coder.agents.watch")

ChatWatchWebSocket::ChatWatchWebSocket(QObject* parent) : WebSocketBase(parent) {
    // Reconnection is managed here so the query-parameter token can be
    // recomputed on every attempt.
    setAutoReconnect(false);
    m_retryTimer.setSingleShot(true);

    connect(this, &WebSocketBase::connected, this, [this]() {
        m_frameParsedSinceConnect = false;
        setState(ConnectionState::Open);
    });
    connect(this, &WebSocketBase::disconnected, this, &ChatWatchWebSocket::handleDisconnected);
    connect(&m_retryTimer, &QTimer::timeout, this, [this]() {
        if (m_active) openConnection();
    });
}

bool ChatWatchWebSocket::websocketsAvailable() {
#ifdef HAS_WEBSOCKETS
    return true;
#else
    return false;
#endif
}

void ChatWatchWebSocket::openWatch() {
    closeWatch();
    m_active = true;
    m_attempt = 0;
    if (!websocketsAvailable()) {
        // Callers detect Failed and fall back to REST polling.
        setState(ConnectionState::Failed);
        return;
    }
    openConnection();
}

void ChatWatchWebSocket::closeWatch() {
    m_active = false;
    m_retryTimer.stop();
    WebSocketBase::disconnect();
    setState(ConnectionState::Disconnected);
}

void ChatWatchWebSocket::reconnectNow() {
    if (!websocketsAvailable()) return;
    m_retryTimer.stop();
    m_active = true;
    m_attempt = 0;
    WebSocketBase::disconnect();
    openConnection();
}

void ChatWatchWebSocket::onTextMessage(const QString& message) {
    QJsonParseError parseError{};
    const QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        qCWarning(lcChatWatch) << "dropping unparseable watch frame";
        return;
    }
    if (!m_frameParsedSinceConnect) {
        m_frameParsedSinceConnect = true;
        m_attempt = 0;
    }
    emit watchEventReceived(ChatWatchEvent::fromJson(doc.object()));
}

void ChatWatchWebSocket::openConnection() {
    setState(m_attempt > 0 ? ConnectionState::Reconnecting : ConnectionState::Connecting);
    connectToEndpoint(buildPath());
}

void ChatWatchWebSocket::handleDisconnected() {
    if (!m_active) {
        setState(ConnectionState::Disconnected);
        return;
    }
    ++m_attempt;
    if (m_attempt > kMaxReconnectAttempts) {
        qCWarning(lcChatWatch) << "watch reconnect budget exhausted";
        m_active = false;
        setState(ConnectionState::Failed);
        return;
    }
    const int delayMs = qMin(m_attempt * 1000, kMaxBackoffMs);
    qCDebug(lcChatWatch) << "scheduling watch reconnect attempt" << m_attempt << "in" << delayMs
                         << "ms";
    setState(ConnectionState::Reconnecting);
    m_retryTimer.start(delayMs);
}

void ChatWatchWebSocket::setState(ConnectionState state) {
    if (m_state == state) return;
    m_state = state;
    emit watchStateChanged();
}

QString ChatWatchWebSocket::buildPath() const {
    QString path = QStringLiteral("/api/experimental/chats/watch");
    const QString token = sessionToken();
    if (!token.isEmpty()) {
        QUrlQuery q;
        q.addQueryItem(QStringLiteral("coder_session_token"), token);
        path += QLatin1Char('?') + q.toString(QUrl::FullyEncoded);
    }
    return path;
}
