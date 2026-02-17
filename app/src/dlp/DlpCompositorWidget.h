#ifndef DLPCOMPOSITORWIDGET_H
#define DLPCOMPOSITORWIDGET_H

#include <QObject>
#include <QSocketNotifier>
#include <QString>
#include <memory>

#include "coder_dlp.h"

/// Qt wrapper for the DLP nested Wayland compositor.
///
/// Integrates the wlroots event loop with Qt's event loop via QSocketNotifier.
/// Only functional on Wayland; gracefully degrades on X11 (isAvailable() == false).
///
/// NOTE: Do NOT run the wlroots event loop on a separate thread — wlroots is not
/// thread-safe.  QSocketNotifier dispatches compositor events on the Qt main thread.
class DlpCompositorWidget : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool available READ isAvailable CONSTANT)
    Q_PROPERTY(bool running READ isRunning NOTIFY runningChanged)
    Q_PROPERTY(int launchedAppCount READ launchedAppCount NOTIFY appCountChanged)

public:
    explicit DlpCompositorWidget(QObject* parent = nullptr);
    ~DlpCompositorWidget() override;

    /// Returns true when the DLP compositor can run (Wayland session detected).
    [[nodiscard]] bool isAvailable() const;

    /// Returns true when the compositor is currently active.
    [[nodiscard]] bool isRunning() const;

    /// Number of applications launched inside the DLP sandbox.
    [[nodiscard]] int launchedAppCount() const;

    /// Set the log level before calling start(). Maps app log levels to compositor levels.
    void setLogLevel(const QString& level);

    /// Start the DLP compositor.  No-op if already running or unavailable.
    /// @return true on success, false on failure or unavailability.
    Q_INVOKABLE bool start();

    /// Stop the DLP compositor and terminate all sandboxed apps.
    Q_INVOKABLE void stop();

    /// Launch an app inside the DLP sandbox.
    /// @return PID of the launched process, or -1 on error.
    Q_INVOKABLE int launchApp(const QString& command, const QString& workspacePath = QString(),
                              bool isolatePid = true, bool isolateIpc = true,
                              bool isolateNetwork = false);

    /// Update the DLP policy from current settings.
    Q_INVOKABLE void updatePolicy(bool clipboardBlockOutgoing, bool clipboardBlockIncoming,
                                  bool screenshotBlock, bool fileSandbox, bool networkSandbox);

signals:
    void runningChanged();
    void appCountChanged();
    void newSurface();
    void errorOccurred(const QString& message);

private:
    // Owned when started; null when stopped.
    coder_dlp_compositor* m_compositor = nullptr;
    std::unique_ptr<QSocketNotifier> m_notifier;
    int m_appCount = 0;
    coder_dlp_log_level m_logLevel = CODER_DLP_LOG_ERROR;

    /// Static callback registered with coder_dlp_on_new_surface().
    static void onNewSurface(coder_dlp_compositor* comp, void* surface, void* data);

    /// Static callback registered with coder_dlp_set_log_callback().
    /// Forwards compositor log messages through the Qt message handler.
    static void onCompositorLog(const char* message, void* data);
};

#endif  // DLPCOMPOSITORWIDGET_H
