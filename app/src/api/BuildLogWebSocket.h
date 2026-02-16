#ifndef BUILDLOGWEBSOCKET_H
#define BUILDLOGWEBSOCKET_H

#include "WebSocketBase.h"

#include <QDateTime>
#include <QString>

/// WebSocket connection for streaming workspace build logs.
///
/// URL pattern:
///   wss://{deployment}/api/v2/workspacebuilds/{buildId}/logs?follow=true&after=0
class BuildLogWebSocket : public WebSocketBase {
    Q_OBJECT

public:
    explicit BuildLogWebSocket(QObject *parent = nullptr);

    /// Connect to a build's log stream.
    Q_INVOKABLE void connectToBuildLogs(const QString &buildId);

signals:
    /// Emitted for each log line received from the server.
    void logReceived(int id, const QString &output, const QString &level,
                     const QString &stage, const QDateTime &createdAt);

    /// Emitted when the build is complete (WebSocket closes normally).
    void buildComplete();

protected:
    void onTextMessage(const QString &message) override;

private:
    QString m_buildId;
};

#endif // BUILDLOGWEBSOCKET_H
