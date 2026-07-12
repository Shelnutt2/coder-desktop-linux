#ifndef CHATSTREAMWEBSOCKET_H
#define CHATSTREAMWEBSOCKET_H

#include <QString>
#include <QTimer>
#include <functional>

#include "api/WebSocketBase.h"
#include "api/dto/ChatStreamEvent.h"

/// Per-chat streaming WebSocket for the experimental Coder Agents API.
///
/// Connects to /api/experimental/chats/{id}/stream?after_id=N. Frames are
/// JSON arrays of ChatStreamEvent (single objects are also accepted; an
/// empty array is a heartbeat). Authentication uses the
/// ?coder_session_token= query parameter because browsers and some proxies
/// cannot attach custom headers to WebSocket upgrades; WebSocketBase's
/// header auth is kept as a harmless second channel.
///
/// The after_id resume cursor is supplied by a provider callback that is
/// re-evaluated on every (re)connect, so reconnects always resume from the
/// caller's latest durable message id.
///
/// Reconnect policy: linear backoff of attempt * 1s capped at 8s, at most 8
/// attempts. The attempt counter resets on the first successfully parsed
/// frame after a connect. Exhausting the budget transitions to Failed until
/// reconnectNow() is called.
///
/// All functionality degrades to safe no-ops when HAS_WEBSOCKETS is off.
class ChatStreamWebSocket : public WebSocketBase {
    Q_OBJECT
    Q_PROPERTY(ConnectionState connectionState READ connectionState NOTIFY streamStateChanged)

public:
    enum class ConnectionState {
        Disconnected,
        Connecting,
        Open,
        Reconnecting,
        Failed,
    };
    Q_ENUM(ConnectionState)

    explicit ChatStreamWebSocket(QObject* parent = nullptr);
    ~ChatStreamWebSocket() override = default;

    /// Returns the resume cursor to use for the next (re)connect.
    void setAfterIdProvider(std::function<qint64()> provider);

    /// Opens the stream for the given chat. Any previous stream is closed.
    void openStream(const QString& chatId);
    /// Closes the stream and disables reconnection.
    void closeStream();
    /// Restarts the connection and resets the reconnect budget.
    Q_INVOKABLE void reconnectNow();

    [[nodiscard]] ConnectionState connectionState() const { return m_state; }
    [[nodiscard]] QString chatId() const { return m_chatId; }

    /// True when this build has WebSocket support compiled in.
    [[nodiscard]] static bool websocketsAvailable();

signals:
    /// Emitted with the parsed events of one frame (heartbeats are dropped).
    void eventsReceived(const QList<ChatStreamEvent>& events);
    void streamStateChanged();

protected:
    void onTextMessage(const QString& message) override;

private:
    void openConnection();
    void handleDisconnected();
    void setState(ConnectionState state);
    [[nodiscard]] QString buildPath() const;

    static constexpr int kMaxReconnectAttempts = 8;
    static constexpr int kMaxBackoffMs = 8000;

    QString m_chatId;
    std::function<qint64()> m_afterIdProvider;
    ConnectionState m_state = ConnectionState::Disconnected;
    QTimer m_retryTimer;
    int m_attempt = 0;
    bool m_active = false;  // true between openStream() and closeStream()
    bool m_frameParsedSinceConnect = false;
};

#endif  // CHATSTREAMWEBSOCKET_H
