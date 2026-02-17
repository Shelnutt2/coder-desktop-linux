#include "BuildLogWebSocket.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QLoggingCategory>

Q_DECLARE_LOGGING_CATEGORY(lcWebSocket)

BuildLogWebSocket::BuildLogWebSocket(QObject* parent) : WebSocketBase(parent) {
    // When the server finishes sending logs it closes the socket normally.
    QObject::connect(this, &WebSocketBase::disconnected, this, &BuildLogWebSocket::buildComplete);
}

void BuildLogWebSocket::connectToBuildLogs(const QString& buildId) {
    m_buildId = buildId;

    const QString path =
        QStringLiteral("/api/v2/workspacebuilds/%1/logs?follow=true&after=0").arg(m_buildId);

    connectToEndpoint(path);
}

void BuildLogWebSocket::onTextMessage(const QString& message) {
    QJsonParseError parseError{};
    const QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8(), &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        qCWarning(lcWebSocket) << "BuildLogWebSocket: failed to parse JSON:"
                               << parseError.errorString();
        return;
    }

    const QJsonObject obj = doc.object();

    const int id = obj.value(QStringLiteral("id")).toInt();
    const QString output = obj.value(QStringLiteral("output")).toString();
    const QString level = obj.value(QStringLiteral("level")).toString();
    const QString stage = obj.value(QStringLiteral("stage")).toString();
    const QDateTime createdAt =
        QDateTime::fromString(obj.value(QStringLiteral("created_at")).toString(), Qt::ISODate);

    emit logReceived(id, output, level, stage, createdAt);
}
