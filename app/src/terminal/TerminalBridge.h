#ifndef TERMINALBRIDGE_H
#define TERMINALBRIDGE_H

#include "api/TerminalWebSocket.h"

#include <QObject>
#include <QString>

/// Bridge between xterm.js (running in a WebEngineView via QWebChannel)
/// and the C++ TerminalWebSocket PTY connection.
///
/// Registered as a QML type so TerminalPage.qml can instantiate it, then
/// exposed to JavaScript as "terminalBridge" through a WebChannel.
class TerminalBridge : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool connected READ isConnected NOTIFY connectionChanged)
    Q_PROPERTY(bool webSocketsAvailable READ webSocketsAvailable CONSTANT)

public:
    explicit TerminalBridge(QObject* parent = nullptr);

    /// Whether WebSocket support was compiled in (HAS_WEBSOCKETS).
    [[nodiscard]] static bool webSocketsAvailable();

    /// Set deployment credentials (called from QML before the WebEngine connects).
    Q_INVOKABLE void setCredentials(const QString& baseUrl, const QString& token);

    /// Set the target workspace agent ID (called from QML).
    Q_INVOKABLE void setAgentId(const QString& agentId);

    /// Called from JS via QWebChannel when the xterm.js terminal is ready.
    Q_INVOKABLE void notifyReady(int cols, int rows);

    /// Called from JS: user keyboard input.
    Q_INVOKABLE void sendInput(const QString& data);

    /// Called from JS: terminal was resized.
    Q_INVOKABLE void resize(int cols, int rows);

    /// Tear down the WebSocket connection (called from QML on destruction).
    Q_INVOKABLE void disconnectFromPty();

    [[nodiscard]] bool isConnected() const;

signals:
    /// Emitted with base64-encoded terminal output (consumed by JS).
    void outputReceived(const QString& base64Data);
    void connectionChanged();

private:
    void tryConnect();

    TerminalWebSocket m_ws;
    QString m_agentId;
    bool m_jsReady = false;
    bool m_hasPendingResize = false;
    int m_pendingCols = 80;
    int m_pendingRows = 24;
};

#endif  // TERMINALBRIDGE_H
