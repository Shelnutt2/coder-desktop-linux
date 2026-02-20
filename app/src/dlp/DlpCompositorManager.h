#ifndef DLPCOMPOSITORMANAGER_H
#define DLPCOMPOSITORMANAGER_H

#include <QObject>
#include <QSocketNotifier>
#include <QString>
#include <QTimer>
#include <memory>
#include <vector>

#include "coder_dlp.h"

class RunningAppModel;

/// Represents a single running sandboxed app with its own compositor instance.
struct RunningApp {
    int pid = -1;
    QString appName;
    QString command;
    coder_dlp_compositor* compositor = nullptr;  // owned; freed via coder_dlp_destroy
    std::unique_ptr<QSocketNotifier> notifier;
    bool bwrapExited = false;
};

/// Manages per-app DLP compositor instances for isolation.
///
/// Each launched app gets its own nested Wayland compositor.  When the app
/// exits, its compositor is destroyed automatically.
///
/// NOTE: Do NOT run the wlroots event loop on a separate thread — wlroots is not
/// thread-safe.  QSocketNotifier dispatches compositor events on the Qt main thread.
class DlpCompositorManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool available READ isAvailable CONSTANT)
    Q_PROPERTY(bool isWaylandSession READ isWaylandSession CONSTANT)
    Q_PROPERTY(QString securityLevel READ securityLevel CONSTANT)
    Q_PROPERTY(bool running READ isRunning NOTIFY runningChanged)
    Q_PROPERTY(int launchedAppCount READ launchedAppCount NOTIFY appCountChanged)
    Q_PROPERTY(RunningAppModel* runningApps READ runningApps CONSTANT)

public:
    explicit DlpCompositorManager(QObject* parent = nullptr);
    ~DlpCompositorManager() override;

    /// Returns true when the DLP compositor can run (Wayland or X11 session detected).
    [[nodiscard]] bool isAvailable() const;

    /// Returns true when the host session is native Wayland (full DLP protection).
    [[nodiscard]] bool isWaylandSession() const;

    /// Returns the DLP security level based on the host display server:
    /// - "full"        — native Wayland host (all protections enforced)
    /// - "partial"     — X11 host (clipboard isolated, screenshot protection limited)
    /// - "unavailable" — no display server detected
    [[nodiscard]] QString securityLevel() const;

    /// Returns true when any compositor is currently active.
    [[nodiscard]] bool isRunning() const;

    /// Number of applications currently running inside DLP sandboxes.
    [[nodiscard]] int launchedAppCount() const;

    /// Model of running apps for QML consumption.
    [[nodiscard]] RunningAppModel* runningApps() const;

    /// Set the log level for new compositor instances.
    void setLogLevel(const QString& level);

    /// Launch an app in its own isolated compositor.
    /// @param command The command to execute
    /// @param appName Display name (for running apps list)
    /// @param workspacePath Optional workspace path to bind-mount
    /// @param isolatePid PID namespace isolation
    /// @param isolateIpc IPC namespace isolation
    /// @param isolateNetwork Network namespace isolation
    /// @param isolateFilesystem Filesystem isolation
    /// @param bindHomeRw Bind $HOME read-write even with FS isolation
    /// @param extraBindPaths Additional paths to bind-mount
    /// @return PID of launched process, or -1 on error
    Q_INVOKABLE int launchApp(const QString& command, const QString& appName = QString(),
                              const QString& workspacePath = QString(), bool isolatePid = true,
                              bool isolateIpc = true, bool isolateNetwork = false,
                              bool isolateFilesystem = false, bool bindHomeRw = false,
                              const QStringList& extraBindPaths = QStringList());

    /// Stop a specific app by PID.
    Q_INVOKABLE void stopApp(int pid);

    /// Stop all running apps and destroy all compositors.
    Q_INVOKABLE void stopAll();

    /// Update DLP policy on all active compositors.
    Q_INVOKABLE void updatePolicy(bool clipboardBlockOutgoing, bool clipboardBlockIncoming,
                                  bool screenshotBlock, bool fileSandbox, bool networkSandbox,
                                  bool watermarkEnabled);

    /// Set the watermark identity string on all active compositors.
    /// Only effective when policy.watermark_enabled is true.
    Q_INVOKABLE void setWatermarkIdentity(const QString& identity);

signals:
    void runningChanged();
    void appCountChanged();
    void errorOccurred(const QString& message);

private:
    void checkChildProcesses();
    void removeApp(int index);

    /// Static callback for compositor log messages.
    static void onCompositorLog(const char* message, void* data);

    std::vector<std::unique_ptr<RunningApp>> m_apps;
    std::unique_ptr<RunningAppModel> m_runningAppModel;
    QTimer m_processMonitor;  // 1s timer to check waitpid
    coder_dlp_log_level m_logLevel = CODER_DLP_LOG_ERROR;
    coder_dlp_policy m_currentPolicy{};
    QByteArray m_watermarkIdentity;
};

#endif  // DLPCOMPOSITORMANAGER_H
