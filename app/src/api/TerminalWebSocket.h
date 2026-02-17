#ifndef TERMINALWEBSOCKET_H
#define TERMINALWEBSOCKET_H

#include "WebSocketBase.h"

#include <QString>

/// WebSocket connection to a workspace agent's PTY.
///
/// URL pattern:
///   wss://{deployment}/api/v2/workspaceagents/{agentId}/pty
///       ?reconnect={sessionId}&width={cols}&height={rows}
class TerminalWebSocket : public WebSocketBase {
    Q_OBJECT

public:
    explicit TerminalWebSocket(QObject* parent = nullptr);

    /// Connect to an agent's PTY session.
    /// @param agentId  The workspace agent UUID.
    /// @param cols     Terminal width  (default 80).
    /// @param rows     Terminal height (default 24).
    Q_INVOKABLE void connectToPty(const QString& agentId, int cols = 80, int rows = 24);

    /// Send input text to the terminal.
    Q_INVOKABLE void sendInput(const QString& data);

    /// Resize the remote terminal.
    Q_INVOKABLE void resize(int cols, int rows);

    /// Get the session ID (used for reconnection).
    [[nodiscard]] QString sessionId() const;

signals:
    /// Emitted when terminal output is received.
    void outputReceived(const QString& data);

protected:
    void onTextMessage(const QString& message) override;
    void onBinaryMessage(const QByteArray& data) override;

private:
    QString m_sessionId;
    QString m_agentId;
    int m_cols = 80;
    int m_rows = 24;
};

#endif  // TERMINALWEBSOCKET_H
