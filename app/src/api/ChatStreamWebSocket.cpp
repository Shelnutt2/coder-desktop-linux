#include "api/ChatStreamWebSocket.h"

#include <QLoggingCategory>
#include <QUrlQuery>

Q_LOGGING_CATEGORY(lcChatStream, "coder.agents.stream")

ChatStreamWebSocket::ChatStreamWebSocket(QObject* parent) : WebSocketBase(parent) {
    // Reconnection is managed here so the resume cursor can be recomputed
    // on every attempt; the base policy would replay a stale URL.
    setAutoReconnect(false);
    m_retryTimer.setSingleShot(true);

    connect(this, &WebSocketBase::connected, this, [this]() {
        m_frameParsedSinceConnect = false;
        setState(ConnectionState::Open);
    });
    connect(this, &WebSocketBase::disconnected, this, &ChatStreamWebSocket::handleDisconnected);
    connect(&m_retryTimer, &QTimer::timeout, this, [this]() {
        if (m_active) openConnection();
    });
}

bool ChatStreamWebSocket::websocketsAvailable() {
#ifdef HAS_WEBSOCKETS
    return true;
#else
    return false;
#endif
}

void ChatStreamWebSocket::setAfterIdProvider(std::function<qint64()> provider) {
    m_afterIdProvider = std::move(provider);
}

void ChatStreamWebSocket::openStream(const QString& chatId) {
    closeStream();
    m_chatId = chatId;
    m_active = true;
    m_attempt = 0;
    if (!websocketsAvailable()) {
        // Callers detect Failed and fall back to REST polling.
        setState(ConnectionState::Failed);
        return;
    }
    openOrDeferConnection();
}

void ChatStreamWebSocket::closeStream() {
    m_active = false;
    m_reopenPending = false;
    m_retryTimer.stop();
    WebSocketBase::disconnect();
    setState(ConnectionState::Disconnected);
}

void ChatStreamWebSocket::reconnectNow() {
    if (m_chatId.isEmpty() || !websocketsAvailable()) return;
    m_retryTimer.stop();
    m_active = true;
    m_attempt = 0;
    WebSocketBase::disconnect();
    openOrDeferConnection();
}

void ChatStreamWebSocket::onTextMessage(const QString& message) {
    bool ok = false;
    const QList<ChatStreamEvent> events = parseChatStreamFrame(message, &ok);
    if (!ok) {
        qCWarning(lcChatStream) << "dropping unparseable stream frame for chat" << m_chatId;
        return;
    }
    // Any successfully parsed frame (including an empty heartbeat array)
    // proves the connection is healthy, so the reconnect budget resets.
    if (!m_frameParsedSinceConnect) {
        m_frameParsedSinceConnect = true;
        m_attempt = 0;
    }
    // Self-heal: a flowing frame proves the connection is open even if the
    // state machine missed the connected transition (e.g. a reopen raced a
    // close handshake and left the state at Reconnecting).
    setState(ConnectionState::Open);
    if (!events.isEmpty()) emit eventsReceived(events);
}

void ChatStreamWebSocket::openOrDeferConnection() {
    // QWebSocket::open() fails while the previous connection is still
    // closing, which would strand the state machine. Defer the reopen to
    // handleDisconnected() until the socket reaches UnconnectedState.
    if (socketBusy()) {
        m_reopenPending = true;
        setState(m_attempt > 0 ? ConnectionState::Reconnecting : ConnectionState::Connecting);
        return;
    }
    openConnection();
}

void ChatStreamWebSocket::openConnection() {
    setState(m_attempt > 0 ? ConnectionState::Reconnecting : ConnectionState::Connecting);
    connectToEndpoint(buildPath());
}

void ChatStreamWebSocket::handleDisconnected() {
    if (m_reopenPending) {
        // A reopen was requested while the socket was still closing; the
        // socket is now unconnected, so open immediately without burning a
        // reconnect attempt.
        m_reopenPending = false;
        if (m_active) openConnection();
        return;
    }
    if (!m_active) {
        setState(ConnectionState::Disconnected);
        return;
    }
    ++m_attempt;
    if (m_attempt > kMaxReconnectAttempts) {
        qCWarning(lcChatStream) << "stream reconnect budget exhausted for chat" << m_chatId;
        m_active = false;
        setState(ConnectionState::Failed);
        return;
    }
    const int delayMs = qMin(m_attempt * 1000, kMaxBackoffMs);
    qCDebug(lcChatStream) << "scheduling stream reconnect attempt" << m_attempt << "in" << delayMs
                          << "ms";
    setState(ConnectionState::Reconnecting);
    m_retryTimer.start(delayMs);
}

void ChatStreamWebSocket::setState(ConnectionState state) {
    if (m_state == state) return;
    m_state = state;
    emit streamStateChanged();
}

QString ChatStreamWebSocket::buildPath() const {
    QUrlQuery q;
    // The cursor provider is re-evaluated on every (re)connect so the stream
    // resumes from the latest durable message id.
    if (m_afterIdProvider) {
        const qint64 afterId = m_afterIdProvider();
        if (afterId > 0) q.addQueryItem(QStringLiteral("after_id"), QString::number(afterId));
    }

    QString path = QStringLiteral("/api/experimental/chats/%1/stream").arg(m_chatId);
    if (!q.isEmpty()) path += QLatin1Char('?') + q.toString(QUrl::FullyEncoded);
    return path;
}
