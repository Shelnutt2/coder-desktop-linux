#ifndef POLLINGCONTROLLER_H
#define POLLINGCONTROLLER_H

#include <QJsonArray>
#include <QList>
#include <QObject>
#include <QString>
#include <QTimer>

#include "models/TaskModel.h"
#include "models/WorkspaceModel.h"

class CoderApiClient;
class NotificationManager;
class SettingsManager;

/// Orchestrates periodic polling of workspaces/tasks, persistent disk caching,
/// and status-change desktop notifications.
///
/// Typical ownership:
/// @code
///   auto *polling = new PollingController(api, wsModel, taskModel,
///                                         notifMgr, settings, this);
///   polling->start();   // loads cache, fetches immediately, starts timer
/// @endcode
///
/// The controller connects to CoderApiClient::workspacesReceived /
/// tasksReceived and drives WorkspaceModel / TaskModel updates.  It also
/// detects status changes between poll cycles and routes desktop
/// notifications through NotificationManager (gated by
/// SettingsManager::notificationsEnabled()).
class PollingController : public QObject {
    Q_OBJECT
    Q_PROPERTY(int refreshIntervalSec READ refreshIntervalSec WRITE setRefreshIntervalSec NOTIFY
                   refreshIntervalChanged)
    Q_PROPERTY(bool polling READ isPolling NOTIFY pollingChanged)

public:
    /// All references must outlive this controller.
    explicit PollingController(CoderApiClient& api, WorkspaceModel& workspaces, TaskModel& tasks,
                               NotificationManager& notifications, SettingsManager& settings,
                               QObject* parent = nullptr);

    [[nodiscard]] int refreshIntervalSec() const;
    void setRefreshIntervalSec(int sec);
    [[nodiscard]] bool isPolling() const;

public slots:
    /// Load cache from disk (if enabled), issue an immediate API fetch,
    /// and start the periodic poll timer.
    void start();

    /// Stop the periodic poll timer.
    void stop();

    /// Trigger an immediate workspace + task fetch (callable from QML).
    Q_INVOKABLE void refreshNow();

    /// Slot connected to CoderApiClient::workspacesReceived().
    void handleWorkspacesReceived(const QJsonArray& arr);

    /// Slot connected to CoderApiClient::tasksReceived().
    void handleTasksReceived(const QJsonArray& arr);

signals:
    void refreshIntervalChanged();
    void pollingChanged();

private:
    // -- Change detection (for notifications) --------------------------------
    void detectWorkspaceChanges(const QList<WorkspaceModel::WorkspaceInfo>& newList);
    void detectTaskChanges(const QList<TaskModel::TaskInfo>& newList);

    // -- Persistent cache I/O ------------------------------------------------
    void loadCache();
    void saveWorkspaceCache(const QJsonArray& arr);
    void saveTaskCache(const QJsonArray& arr);
    void purgeCache();
    [[nodiscard]] QString cacheDirForDeployment() const;

    // -- Dependencies (non-owning references) --------------------------------
    CoderApiClient& m_api;
    WorkspaceModel& m_workspaceModel;
    TaskModel& m_taskModel;
    NotificationManager& m_notifications;
    SettingsManager& m_settings;

    QTimer m_pollTimer;
    int m_refreshIntervalSec = 10;

    /// Suppresses notifications on the very first data load so the user is
    /// not spammed with "changed" alerts for every existing workspace/task.
    bool m_firstFetch = true;
};

#endif  // POLLINGCONTROLLER_H
