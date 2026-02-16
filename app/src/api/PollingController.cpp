#include "api/PollingController.h"

#include "api/CoderApiClient.h"
#include "models/TaskModel.h"
#include "models/WorkspaceModel.h"
#include "notifications/NotificationManager.h"
#include "settings/SettingsManager.h"

#include <QCryptographicHash>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QSaveFile>
#include <QStandardPaths>

#include <algorithm>

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

PollingController::PollingController(CoderApiClient &api,
                                     WorkspaceModel &workspaces,
                                     TaskModel &tasks,
                                     NotificationManager &notifications,
                                     SettingsManager &settings,
                                     QObject *parent)
    : QObject(parent)
    , m_api(api)
    , m_workspaceModel(workspaces)
    , m_taskModel(tasks)
    , m_notifications(notifications)
    , m_settings(settings)
{
    // Wire timer to periodic refresh.
    connect(&m_pollTimer, &QTimer::timeout, this,
            &PollingController::refreshNow);

    // Wire API result signals to our handlers.
    connect(&m_api, &CoderApiClient::workspacesReceived, this,
            &PollingController::handleWorkspacesReceived);
    connect(&m_api, &CoderApiClient::tasksReceived, this,
            &PollingController::handleTasksReceived);

    // React to settings changes (interval, cache toggle).
    connect(&m_settings, &SettingsManager::settingsChanged, this, [this]() {
        setRefreshIntervalSec(m_settings.refreshIntervalSec());
        if (m_settings.disableDataCache()) {
            purgeCache();
        }
    });

    m_pollTimer.setInterval(m_refreshIntervalSec * 1000);
}

// ---------------------------------------------------------------------------
// Properties
// ---------------------------------------------------------------------------

int PollingController::refreshIntervalSec() const
{
    return m_refreshIntervalSec;
}

void PollingController::setRefreshIntervalSec(int sec)
{
    // Clamp to a sane range.
    constexpr int kMinInterval = 5;
    constexpr int kMaxInterval = 300;
    sec = std::clamp(sec, kMinInterval, kMaxInterval);

    if (sec == m_refreshIntervalSec) {
        return;
    }
    m_refreshIntervalSec = sec;
    m_pollTimer.setInterval(m_refreshIntervalSec * 1000);
    emit refreshIntervalChanged();
}

bool PollingController::isPolling() const
{
    return m_pollTimer.isActive();
}

// ---------------------------------------------------------------------------
// start / stop / refreshNow
// ---------------------------------------------------------------------------

void PollingController::start()
{
    m_firstFetch = true;
    loadCache();
    refreshNow();
    m_pollTimer.start();
    emit pollingChanged();
}

void PollingController::stop()
{
    m_pollTimer.stop();
    emit pollingChanged();
}

void PollingController::refreshNow()
{
    m_api.fetchWorkspaces();
    m_api.fetchTasks();
}

// ---------------------------------------------------------------------------
// API result handlers
// ---------------------------------------------------------------------------

void PollingController::handleWorkspacesReceived(const QJsonArray &arr)
{
    QList<WorkspaceModel::WorkspaceInfo> list;
    list.reserve(arr.size());
    for (const auto &v : arr) {
        list.append(WorkspaceModel::WorkspaceInfo::fromJson(v.toObject()));
    }

    // Detect changes *before* updating the model (so we can compare old state).
    detectWorkspaceChanges(list);

    m_workspaceModel.setWorkspaces(list);
    m_workspaceModel.setLoading(false);
    m_workspaceModel.setErrorMessage(QString());

    saveWorkspaceCache(arr);
    m_firstFetch = false;
}

void PollingController::handleTasksReceived(const QJsonArray &arr)
{
    QList<TaskModel::TaskInfo> list;
    list.reserve(arr.size());
    for (const auto &v : arr) {
        list.append(TaskModel::TaskInfo::fromJson(v.toObject()));
    }

    detectTaskChanges(list);

    m_taskModel.setTasks(list);
    m_taskModel.setLoading(false);
    m_taskModel.setErrorMessage(QString());

    saveTaskCache(arr);
    // Note: m_firstFetch is cleared by handleWorkspacesReceived; tasks arriving
    // after the first workspace response will already have notifications enabled.
}

// ---------------------------------------------------------------------------
// Change detection → desktop notifications
// ---------------------------------------------------------------------------

void PollingController::detectWorkspaceChanges(
    const QList<WorkspaceModel::WorkspaceInfo> &newList)
{
    if (m_firstFetch || !m_settings.notificationsEnabled()) {
        return;
    }

    // Build a map of the *current* model state (before we overwrite it).
    QHash<QString, int> oldStatus;
    for (int i = 0; i < m_workspaceModel.rowCount(); ++i) {
        const auto idx = m_workspaceModel.index(i);
        oldStatus[idx.data(WorkspaceModel::IdRole).toString()] =
            idx.data(WorkspaceModel::StatusRole).toInt();
    }

    for (const auto &ws : newList) {
        if (!oldStatus.contains(ws.id)) {
            continue; // brand-new workspace — no prior state to compare
        }
        if (oldStatus.value(ws.id) != ws.status) {
            m_notifications.notify(
                QStringLiteral("Workspace: %1").arg(ws.name),
                QStringLiteral("Status changed"),
                QStringLiteral("WorkspaceState"));
        }
    }
}

void PollingController::detectTaskChanges(
    const QList<TaskModel::TaskInfo> &newList)
{
    if (m_firstFetch || !m_settings.notificationsEnabled()) {
        return;
    }

    QHash<QString, int> oldStatus;
    QHash<QString, int> oldState;
    for (int i = 0; i < m_taskModel.rowCount(); ++i) {
        const auto idx = m_taskModel.index(i);
        const auto id = idx.data(TaskModel::IdRole).toString();
        oldStatus[id] = idx.data(TaskModel::StatusRole).toInt();
        oldState[id] = idx.data(TaskModel::CurrentStateRole).toInt();
    }

    for (const auto &task : newList) {
        if (!oldStatus.contains(task.id)) {
            continue; // new task
        }
        if (oldStatus.value(task.id) != task.status ||
            oldState.value(task.id) != task.currentState) {
            const auto &label =
                task.displayName.isEmpty() ? task.name : task.displayName;
            m_notifications.notify(QStringLiteral("Task: %1").arg(label),
                                   QStringLiteral("Status changed"),
                                   QStringLiteral("TaskUpdate"));
        }
    }
}

// ---------------------------------------------------------------------------
// Persistent cache helpers
// ---------------------------------------------------------------------------

QString PollingController::cacheDirForDeployment() const
{
    const auto cacheRoot =
        QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    const auto urlHash = QString::fromLatin1(
        QCryptographicHash::hash(m_api.baseUrl().toUtf8(),
                                 QCryptographicHash::Sha1)
            .toHex());
    const auto dir = cacheRoot + QLatin1Char('/') + urlHash;
    QDir().mkpath(dir);
    return dir;
}

void PollingController::loadCache()
{
    if (m_settings.disableDataCache()) {
        return;
    }

    const auto dir = cacheDirForDeployment();

    // -- Workspaces --
    {
        QFile f(dir + QStringLiteral("/workspaces.json"));
        if (f.open(QIODevice::ReadOnly)) {
            const auto doc = QJsonDocument::fromJson(f.readAll());
            if (doc.isArray()) {
                QList<WorkspaceModel::WorkspaceInfo> list;
                const auto arr = doc.array();
                list.reserve(arr.size());
                for (const auto &v : arr) {
                    list.append(
                        WorkspaceModel::WorkspaceInfo::fromJson(v.toObject()));
                }
                m_workspaceModel.setWorkspaces(list);
                qDebug() << "[PollingController] loaded" << list.size()
                         << "workspaces from cache";
            }
        }
    }

    // -- Tasks --
    {
        QFile f(dir + QStringLiteral("/tasks.json"));
        if (f.open(QIODevice::ReadOnly)) {
            const auto doc = QJsonDocument::fromJson(f.readAll());
            if (doc.isArray()) {
                QList<TaskModel::TaskInfo> list;
                const auto arr = doc.array();
                list.reserve(arr.size());
                for (const auto &v : arr) {
                    list.append(
                        TaskModel::TaskInfo::fromJson(v.toObject()));
                }
                m_taskModel.setTasks(list);
                qDebug() << "[PollingController] loaded" << list.size()
                         << "tasks from cache";
            }
        }
    }
}

void PollingController::saveWorkspaceCache(const QJsonArray &arr)
{
    if (m_settings.disableDataCache()) {
        return;
    }

    const auto path =
        cacheDirForDeployment() + QStringLiteral("/workspaces.json");
    QSaveFile f(path);
    if (f.open(QIODevice::WriteOnly)) {
        f.write(QJsonDocument(arr).toJson(QJsonDocument::Compact));
        if (!f.commit()) {
            qWarning() << "[PollingController] failed to commit workspace cache"
                       << path;
        }
    } else {
        qWarning() << "[PollingController] failed to open workspace cache for writing"
                   << path;
    }
}

void PollingController::saveTaskCache(const QJsonArray &arr)
{
    if (m_settings.disableDataCache()) {
        return;
    }

    const auto path =
        cacheDirForDeployment() + QStringLiteral("/tasks.json");
    QSaveFile f(path);
    if (f.open(QIODevice::WriteOnly)) {
        f.write(QJsonDocument(arr).toJson(QJsonDocument::Compact));
        if (!f.commit()) {
            qWarning() << "[PollingController] failed to commit task cache"
                       << path;
        }
    } else {
        qWarning() << "[PollingController] failed to open task cache for writing"
                   << path;
    }
}

void PollingController::purgeCache()
{
    const auto dir = cacheDirForDeployment();
    for (const auto &name :
         {QStringLiteral("workspaces.json"), QStringLiteral("tasks.json")}) {
        const auto path = dir + QLatin1Char('/') + name;
        if (QFile::exists(path)) {
            if (QFile::remove(path)) {
                qDebug() << "[PollingController] purged cache file" << path;
            } else {
                qWarning() << "[PollingController] failed to remove cache file"
                           << path;
            }
        }
    }
}
