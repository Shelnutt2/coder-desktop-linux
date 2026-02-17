#include "TerminalWebSocket.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QUuid>

TerminalWebSocket::TerminalWebSocket(QObject* parent) : WebSocketBase(parent) {}

void TerminalWebSocket::connectToPty(const QString& agentId, int cols, int rows) {
    m_agentId = agentId;
    m_cols = cols;
    m_rows = rows;

    // Generate a stable session ID so reconnects resume the same PTY.
    if (m_sessionId.isEmpty()) {
        m_sessionId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    }

    const QString path =
        QStringLiteral("/api/v2/workspaceagents/%1/pty?reconnect=%2&width=%3&height=%4")
            .arg(m_agentId, m_sessionId)
            .arg(m_cols)
            .arg(m_rows);

    connectToEndpoint(path);
}

void TerminalWebSocket::sendInput(const QString& data) {
    QJsonObject msg;
    msg[QStringLiteral("data")] = data;
    // Coder PTY API expects binary WebSocket frames (matching Android client).
    sendBinaryMessage(QJsonDocument(msg).toJson(QJsonDocument::Compact));
}

void TerminalWebSocket::resize(int cols, int rows) {
    m_cols = cols;
    m_rows = rows;

    QJsonObject msg;
    msg[QStringLiteral("width")] = cols;
    msg[QStringLiteral("height")] = rows;
    // Coder PTY API expects binary WebSocket frames (matching Android client).
    sendBinaryMessage(QJsonDocument(msg).toJson(QJsonDocument::Compact));
}

QString TerminalWebSocket::sessionId() const {
    return m_sessionId;
}

void TerminalWebSocket::onTextMessage(const QString& message) {
    emit outputReceived(message);
}

void TerminalWebSocket::onBinaryMessage(const QByteArray& data) {
    emit outputReceived(QString::fromUtf8(data));
}
