#ifndef WEBSOCKETBASE_H
#define WEBSOCKETBASE_H

#include <QObject>
#include <QString>
#include <QTimer>
#include <QUrl>

#ifdef HAS_WEBSOCKETS
#include <QWebSocket>
#endif

/// Base class for authenticated Coder WebSocket connections.
/// Handles connection lifecycle, auth header injection, and auto-reconnect.
///
/// All functionality is guarded behind HAS_WEBSOCKETS.  When the define is
/// absent every public method is a safe no-op that logs a warning.
class WebSocketBase : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool connected READ isConnected NOTIFY connectionStateChanged)

public:
    explicit WebSocketBase(QObject* parent = nullptr);
    ~WebSocketBase() override;

    void setBaseUrl(const QString& url);
    void setSessionToken(const QString& token);

    [[nodiscard]] bool isConnected() const;

    Q_INVOKABLE void connectToEndpoint(const QString& path);
    Q_INVOKABLE void disconnect();

    // Reconnection policy
    void setAutoReconnect(bool enabled);
    void setMaxReconnectAttempts(int max);  // default 5

signals:
    void connectionStateChanged();
    void connected();
    void disconnected();
    void errorOccurred(const QString& error);
    void textMessageReceived(const QString& message);
    void binaryMessageReceived(const QByteArray& data);

protected:
    void sendTextMessage(const QString& message);
    void sendBinaryMessage(const QByteArray& data);

    /// Subclasses override to handle incoming text frames.
    virtual void onTextMessage(const QString& message);
    /// Subclasses override to handle incoming binary frames.
    virtual void onBinaryMessage(const QByteArray& data);

private:
#ifdef HAS_WEBSOCKETS
    QWebSocket m_socket;
#endif
    QString m_baseUrl;
    QString m_sessionToken;
    bool m_autoReconnect = true;
    int m_maxReconnectAttempts = 5;
    int m_reconnectAttempt = 0;
    QTimer m_reconnectTimer;
    QString m_lastPath;

    void onConnected();
    void onDisconnected();
    void onError(int error);
    void attemptReconnect();
    [[nodiscard]] QUrl buildWsUrl(const QString& path) const;
};

#endif  // WEBSOCKETBASE_H
