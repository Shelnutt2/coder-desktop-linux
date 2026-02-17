#include "TerminalBridge.h"

#include <QByteArray>

TerminalBridge::TerminalBridge(QObject* parent) : QObject(parent) {
    // Forward WebSocket output as base64 to JS (binary-safe transport
    // over QWebChannel which only supports strings).
    connect(&m_ws, &TerminalWebSocket::outputReceived, this, [this](const QString& data) {
        emit outputReceived(QString::fromLatin1(data.toUtf8().toBase64()));
    });

    connect(&m_ws, &WebSocketBase::connectionStateChanged, this, [this]() {
        emit connectionChanged();
        // Flush any resize that arrived before connection was ready.
        if (m_ws.isConnected() && m_hasPendingResize) {
            m_hasPendingResize = false;
            m_ws.resize(m_pendingCols, m_pendingRows);
        }
    });
}

void TerminalBridge::setCredentials(const QString& baseUrl, const QString& token) {
    m_ws.setBaseUrl(baseUrl);
    m_ws.setSessionToken(token);
}

void TerminalBridge::setAgentId(const QString& agentId) {
    m_agentId = agentId;
    tryConnect();
}

void TerminalBridge::notifyReady(int cols, int rows) {
    m_jsReady = true;
    m_pendingCols = cols;
    m_pendingRows = rows;
    tryConnect();
}

void TerminalBridge::tryConnect() {
    if (m_jsReady && !m_agentId.isEmpty()) {
        m_ws.connectToPty(m_agentId, m_pendingCols, m_pendingRows);
    }
}

void TerminalBridge::sendInput(const QString& data) {
    if (m_ws.isConnected()) {
        m_ws.sendInput(data);
    }
    // Silently drop input before connection — no point queueing keystrokes.
}

void TerminalBridge::resize(int cols, int rows) {
    m_pendingCols = cols;
    m_pendingRows = rows;
    if (m_ws.isConnected()) {
        m_ws.resize(cols, rows);
    } else {
        // Will be sent when connectionStateChanged fires.
        m_hasPendingResize = true;
    }
}

void TerminalBridge::disconnectFromPty() {
    m_ws.disconnect();
}

bool TerminalBridge::isConnected() const {
    return m_ws.isConnected();
}

bool TerminalBridge::webSocketsAvailable() {
#ifdef HAS_WEBSOCKETS
    return true;
#else
    return false;
#endif
}
