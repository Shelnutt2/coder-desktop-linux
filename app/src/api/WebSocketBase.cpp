#include "WebSocketBase.h"

#include <QLoggingCategory>

#ifdef HAS_WEBSOCKETS
#include <QNetworkRequest>
#endif

Q_LOGGING_CATEGORY(lcWebSocket, "coder.websocket")

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

WebSocketBase::WebSocketBase(QObject *parent)
    : QObject(parent)
{
    m_reconnectTimer.setSingleShot(true);

#ifdef HAS_WEBSOCKETS
    QObject::connect(&m_socket, &QWebSocket::connected,
                     this, &WebSocketBase::onConnected);
    QObject::connect(&m_socket, &QWebSocket::disconnected,
                     this, &WebSocketBase::onDisconnected);
    QObject::connect(&m_socket, &QWebSocket::textMessageReceived,
                     this, &WebSocketBase::onTextMessage);
    QObject::connect(&m_socket, &QWebSocket::binaryMessageReceived,
                     this, &WebSocketBase::onBinaryMessage);

    // QWebSocket::errorOccurred provides QAbstractSocket::SocketError which
    // is an int-compatible enum.  We wrap it via a lambda so the slot
    // signature stays simple.
    QObject::connect(&m_socket, &QWebSocket::errorOccurred,
                     this, [this](QAbstractSocket::SocketError err) {
                         onError(static_cast<int>(err));
                     });
#endif

    QObject::connect(&m_reconnectTimer, &QTimer::timeout,
                     this, &WebSocketBase::attemptReconnect);
}

WebSocketBase::~WebSocketBase()
{
    disconnect();
}

// ---------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------

void WebSocketBase::setBaseUrl(const QString &url)
{
    m_baseUrl = url;
    // Strip trailing slash for consistent URL building.
    while (m_baseUrl.endsWith(QLatin1Char('/'))) {
        m_baseUrl.chop(1);
    }
}

void WebSocketBase::setSessionToken(const QString &token)
{
    m_sessionToken = token;
}

void WebSocketBase::setAutoReconnect(bool enabled)
{
    m_autoReconnect = enabled;
}

void WebSocketBase::setMaxReconnectAttempts(int max)
{
    m_maxReconnectAttempts = max;
}

// ---------------------------------------------------------------------------
// Connection state
// ---------------------------------------------------------------------------

bool WebSocketBase::isConnected() const
{
#ifdef HAS_WEBSOCKETS
    return m_socket.state() == QAbstractSocket::ConnectedState;
#else
    return false;
#endif
}

// ---------------------------------------------------------------------------
// Connect / disconnect
// ---------------------------------------------------------------------------

void WebSocketBase::connectToEndpoint(const QString &path)
{
#ifdef HAS_WEBSOCKETS
    m_lastPath = path;
    m_reconnectAttempt = 0;
    m_reconnectTimer.stop();

    const QUrl url = buildWsUrl(path);
    if (!url.isValid()) {
        qCWarning(lcWebSocket) << "Invalid WebSocket URL:" << url.toString();
        emit errorOccurred(QStringLiteral("Invalid WebSocket URL"));
        return;
    }

    QNetworkRequest request(url);
    if (!m_sessionToken.isEmpty()) {
        request.setRawHeader("Coder-Session-Token", m_sessionToken.toUtf8());
    }

    qCDebug(lcWebSocket) << "Opening WebSocket to" << url.toString();
    m_socket.open(request);
#else
    Q_UNUSED(path);
    qCWarning(lcWebSocket) << "WebSocket support not compiled (HAS_WEBSOCKETS undefined)";
#endif
}

void WebSocketBase::disconnect()
{
#ifdef HAS_WEBSOCKETS
    m_autoReconnect = false; // explicit disconnect — don't reconnect
    m_reconnectTimer.stop();
    if (m_socket.state() != QAbstractSocket::UnconnectedState) {
        m_socket.close();
    }
#endif
}

// ---------------------------------------------------------------------------
// Send helpers
// ---------------------------------------------------------------------------

void WebSocketBase::sendTextMessage(const QString &message)
{
#ifdef HAS_WEBSOCKETS
    if (isConnected()) {
        m_socket.sendTextMessage(message);
    } else {
        qCWarning(lcWebSocket) << "Cannot send text message — not connected";
    }
#else
    Q_UNUSED(message);
    qCWarning(lcWebSocket) << "WebSocket support not compiled (HAS_WEBSOCKETS undefined)";
#endif
}

void WebSocketBase::sendBinaryMessage(const QByteArray &data)
{
#ifdef HAS_WEBSOCKETS
    if (isConnected()) {
        m_socket.sendBinaryMessage(data);
    } else {
        qCWarning(lcWebSocket) << "Cannot send binary message — not connected";
    }
#else
    Q_UNUSED(data);
    qCWarning(lcWebSocket) << "WebSocket support not compiled (HAS_WEBSOCKETS undefined)";
#endif
}

// ---------------------------------------------------------------------------
// Default message handlers (subclasses override)
// ---------------------------------------------------------------------------

void WebSocketBase::onTextMessage(const QString &message)
{
    emit textMessageReceived(message);
}

void WebSocketBase::onBinaryMessage(const QByteArray &data)
{
    emit binaryMessageReceived(data);
}

// ---------------------------------------------------------------------------
// Internal slots
// ---------------------------------------------------------------------------

void WebSocketBase::onConnected()
{
    qCDebug(lcWebSocket) << "WebSocket connected";
    m_reconnectAttempt = 0;
    emit connected();
    emit connectionStateChanged();
}

void WebSocketBase::onDisconnected()
{
    qCDebug(lcWebSocket) << "WebSocket disconnected";
    emit disconnected();
    emit connectionStateChanged();

    if (m_autoReconnect && m_reconnectAttempt < m_maxReconnectAttempts) {
        const int delayMs = (m_reconnectAttempt + 1) * 1000;
        qCDebug(lcWebSocket) << "Scheduling reconnect attempt"
                             << (m_reconnectAttempt + 1) << "in" << delayMs << "ms";
        m_reconnectTimer.start(delayMs);
    }
}

void WebSocketBase::onError(int error)
{
#ifdef HAS_WEBSOCKETS
    const QString msg = m_socket.errorString();
    qCWarning(lcWebSocket) << "WebSocket error" << error << ":" << msg;
    emit errorOccurred(msg);
#else
    Q_UNUSED(error);
#endif
}

void WebSocketBase::attemptReconnect()
{
    if (m_lastPath.isEmpty()) {
        return;
    }
    ++m_reconnectAttempt;
    const int attempt = m_reconnectAttempt;
    qCDebug(lcWebSocket) << "Reconnect attempt" << attempt
                         << "of" << m_maxReconnectAttempts;
    // connectToEndpoint() resets m_reconnectAttempt, so restore it afterwards.
    connectToEndpoint(m_lastPath);
    m_reconnectAttempt = attempt;
}

// ---------------------------------------------------------------------------
// URL helpers
// ---------------------------------------------------------------------------

QUrl WebSocketBase::buildWsUrl(const QString &path) const
{
    QString base = m_baseUrl;
    if (base.startsWith(QStringLiteral("https://"), Qt::CaseInsensitive)) {
        base.replace(0, 8, QStringLiteral("wss://"));
    } else if (base.startsWith(QStringLiteral("http://"), Qt::CaseInsensitive)) {
        base.replace(0, 7, QStringLiteral("ws://"));
    }

    // Ensure path starts with '/'
    if (!path.startsWith(QLatin1Char('/'))) {
        base += QLatin1Char('/');
    }
    base += path;

    return QUrl(base);
}
