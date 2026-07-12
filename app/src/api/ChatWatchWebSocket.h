#ifndef CHATWATCHWEBSOCKET_H
#define CHATWATCHWEBSOCKET_H

#include <QString>
#include <QTimer>

#include "api/WebSocketBase.h"
#include "api/dto/ChatStreamEvent.h"

/// Global chat lifecycle WebSocket for the experimental Coder Agents API.
///
/// Connects to /api/experimental/chats/watch and receives one
/// ChatWatchEvent JSON object per text frame (created, updated,
/// status_change, title_change, deleted, diff_status_change,
/// action_required, context_dirty). Authentication uses the
/// ?coder_session_token= query parameter.
///
/// Reconnect policy matches ChatStreamWebSocket: linear backoff of
/// attempt * 1s capped at 8s, at most 8 attempts, counter reset on the
/// first successfully parsed frame.
///
/// All functionality degrades to safe no-ops when HAS_WEBSOCKETS is off.
class ChatWatchWebSocket : public WebSocketBase {
    Q_OBJECT
    Q_PROPERTY(ConnectionState connectionState READ connectionState NOTIFY watchStateChanged)

public:
    enum class ConnectionState {
        Disconnected,
        Connecting,
        Open,
        Reconnecting,
        Failed,
    };
    Q_ENUM(ConnectionState)

    explicit ChatWatchWebSocket(QObject* parent = nullptr);
    ~ChatWatchWebSocket() override = default;

    /// Opens the watch stream. Any previous connection is closed.
    void openWatch();
    /// Closes the watch stream and disables reconnection.
    void closeWatch();
    /// Restarts the connection and resets the reconnect budget.
    Q_INVOKABLE void reconnectNow();

    [[nodiscard]] ConnectionState connectionState() const { return m_state; }

    /// True when this build has WebSocket support compiled in.
    [[nodiscard]] static bool websocketsAvailable();

signals:
    void watchEventReceived(const ChatWatchEvent& event);
    void watchStateChanged();

protected:
    void onTextMessage(const QString& message) override;

private:
    void openConnection();
    void handleDisconnected();
    void setState(ConnectionState state);
    [[nodiscard]] QString buildPath() const;

    static constexpr int kMaxReconnectAttempts = 8;
    static constexpr int kMaxBackoffMs = 8000;

    ConnectionState m_state = ConnectionState::Disconnected;
    QTimer m_retryTimer;
    int m_attempt = 0;
    bool m_active = false;  // true between openWatch() and closeWatch()
    bool m_frameParsedSinceConnect = false;
};

#endif  // CHATWATCHWEBSOCKET_H
