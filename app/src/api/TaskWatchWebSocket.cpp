#include "api/TaskWatchWebSocket.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#ifdef HAS_WEBSOCKETS
#include <QDebug>
#endif

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

TaskWatchWebSocket::TaskWatchWebSocket(QObject* parent) : WebSocketBase(parent) {}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void TaskWatchWebSocket::watchWorkspace(const QString& workspaceId) {
    m_workspaceId = workspaceId;
    const QString path = QStringLiteral("/api/v2/workspaces/%1/watch-ws").arg(workspaceId);
    connectToEndpoint(path);
}

void TaskWatchWebSocket::stopWatching() {
    m_workspaceId.clear();
    disconnect();
}

// ---------------------------------------------------------------------------
// WebSocketBase overrides
// ---------------------------------------------------------------------------

void TaskWatchWebSocket::onTextMessage(const QString& message) {
#ifdef HAS_WEBSOCKETS
    const QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8());
    if (!doc.isObject()) return;

    const QJsonObject root = doc.object();
    const QString type = root.value(QLatin1String("type")).toString();

    if (type != QLatin1String("data")) return;

    const QJsonObject workspace = root.value(QLatin1String("data")).toObject();
    if (workspace.isEmpty()) return;

    emit workspaceChanged(workspace);

    // Derive task status from the workspace build state.
    const TaskStatus status = deriveTaskStatus(workspace);

    // Build a TaskStateEntry from latest_build metadata if available.
    TaskStateEntry stateEntry;
    const QJsonObject latestBuild = workspace.value(QLatin1String("latest_build")).toObject();
    const QString buildStatus = latestBuild.value(QLatin1String("status")).toString();
    stateEntry.message = buildStatus;
    const QString ts = latestBuild.value(QLatin1String("updated_at")).toString();
    if (!ts.isEmpty()) stateEntry.timestamp = QDateTime::fromString(ts, Qt::ISODate);

    // Determine state from build status.
    if (buildStatus == QLatin1String("running")) {
        stateEntry.state = TaskState::Working;
    } else if (buildStatus == QLatin1String("failed")) {
        stateEntry.state = TaskState::Failed;
    } else if (buildStatus == QLatin1String("stopped") ||
               buildStatus == QLatin1String("canceled") ||
               buildStatus == QLatin1String("deleted")) {
        stateEntry.state = TaskState::Complete;
    } else {
        stateEntry.state = TaskState::Idle;
    }

    emit taskUpdated(status, stateEntry);
#else
    Q_UNUSED(message)
#endif
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

#ifdef HAS_WEBSOCKETS
TaskStatus TaskWatchWebSocket::deriveTaskStatus(const QJsonObject& workspace) const {
    const QJsonObject latestBuild = workspace.value(QLatin1String("latest_build")).toObject();
    const QString buildStatus = latestBuild.value(QLatin1String("status")).toString();
    const QString transition = latestBuild.value(QLatin1String("transition")).toString();

    // Map workspace build status + transition to a TaskStatus.
    if (buildStatus == QLatin1String("pending")) return TaskStatus::Pending;
    if (buildStatus == QLatin1String("starting")) return TaskStatus::Initializing;
    if (buildStatus == QLatin1String("running")) {
        if (transition == QLatin1String("stop") || transition == QLatin1String("delete"))
            return TaskStatus::Paused;
        return TaskStatus::Active;
    }
    if (buildStatus == QLatin1String("stopping") || buildStatus == QLatin1String("canceling") ||
        buildStatus == QLatin1String("deleting"))
        return TaskStatus::Paused;
    if (buildStatus == QLatin1String("stopped") || buildStatus == QLatin1String("canceled") ||
        buildStatus == QLatin1String("deleted"))
        return TaskStatus::Paused;
    if (buildStatus == QLatin1String("failed")) return TaskStatus::Error;

    return TaskStatus::Unknown;
}
#endif
