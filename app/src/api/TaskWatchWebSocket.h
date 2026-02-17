#ifndef TASKWATCHWEBSOCKET_H
#define TASKWATCHWEBSOCKET_H

#include "api/WebSocketBase.h"
#include "api/dto/Task.h"

#include <QJsonObject>
#include <QString>

/// Watches a workspace via WebSocket for real-time task status updates.
///
/// Connects to: wss://{deployment}/api/v2/workspaces/{workspaceId}/watch-ws
/// Parses workspace watch messages of the form:
///   {"type": "data", "data": {WorkspaceResponse}}
/// and derives TaskStatus from the workspace build status + transition.
///
/// All functionality is guarded behind HAS_WEBSOCKETS.
class TaskWatchWebSocket : public WebSocketBase {
    Q_OBJECT

public:
    explicit TaskWatchWebSocket(QObject* parent = nullptr);
    ~TaskWatchWebSocket() override = default;

    /// Start watching a specific workspace for task updates.
    void watchWorkspace(const QString& workspaceId);

    /// Stop watching and disconnect.
    void stopWatching();

signals:
    /// Emitted when the task status is derived from a workspace update.
    void taskUpdated(TaskStatus status, TaskStateEntry state);

    /// Emitted for every workspace data message (full workspace JSON).
    void workspaceChanged(const QJsonObject& workspace);

protected:
    void onTextMessage(const QString& message) override;

private:
    QString m_workspaceId;

#ifdef HAS_WEBSOCKETS
    [[nodiscard]] TaskStatus deriveTaskStatus(const QJsonObject& workspace) const;
#endif
};

#endif  // TASKWATCHWEBSOCKET_H
