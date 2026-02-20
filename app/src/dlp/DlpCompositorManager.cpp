#include "dlp/DlpCompositorManager.h"

#include <signal.h>
#include <sys/wait.h>

#include <QByteArray>
#include <QLoggingCategory>

#include "apps/RunningAppModel.h"

Q_LOGGING_CATEGORY(lcDlpMgr, "coder.dlp")

DlpCompositorManager::DlpCompositorManager(QObject* parent)
    : QObject(parent), m_runningAppModel(std::make_unique<RunningAppModel>()) {
    // Poll for exited child processes every second.
    connect(&m_processMonitor, &QTimer::timeout, this, &DlpCompositorManager::checkChildProcesses);
    m_processMonitor.start(1000);
}

DlpCompositorManager::~DlpCompositorManager() {
    stopAll();
}

bool DlpCompositorManager::isAvailable() const {
    return coder_dlp_is_available();
}

bool DlpCompositorManager::isWaylandSession() const {
    return qEnvironmentVariableIsSet("WAYLAND_DISPLAY");
}

QString DlpCompositorManager::securityLevel() const {
    if (!isAvailable()) {
        return QStringLiteral("unavailable");
    }
    if (qEnvironmentVariableIsSet("WAYLAND_DISPLAY")) {
        return QStringLiteral("full");
    }
    // If we got here, isAvailable()==true but no WAYLAND_DISPLAY,
    // so we must be on X11 (coder_dlp_is_available checked DISPLAY).
    return QStringLiteral("partial");
}

bool DlpCompositorManager::isRunning() const {
    return !m_apps.empty();
}

int DlpCompositorManager::launchedAppCount() const {
    return static_cast<int>(m_apps.size());
}

RunningAppModel* DlpCompositorManager::runningApps() const {
    return m_runningAppModel.get();
}

void DlpCompositorManager::setLogLevel(const QString& level) {
    if (level == QStringLiteral("trace") || level == QStringLiteral("debug")) {
        m_logLevel = CODER_DLP_LOG_DEBUG;
    } else if (level == QStringLiteral("info")) {
        m_logLevel = CODER_DLP_LOG_INFO;
    } else {
        // "warn", "error", or unrecognized → errors only
        m_logLevel = CODER_DLP_LOG_ERROR;
    }
}

int DlpCompositorManager::launchApp(const QString& command, const QString& appName,
                                    const QString& workspacePath, bool isolatePid, bool isolateIpc,
                                    bool isolateNetwork, bool isolateFilesystem, bool bindHomeRw,
                                    const QStringList& extraBindPaths, bool filterDbus,
                                    const QStringList& dbusAllowedNames) {
    if (!isAvailable()) {
        emit errorOccurred(QStringLiteral("DLP compositor requires a Wayland or X11 session"));
        return -1;
    }

    // Create a dedicated compositor for this app.
    coder_dlp_compositor* comp = coder_dlp_create(nullptr, m_logLevel);
    if (!comp) {
        emit errorOccurred(QStringLiteral("Failed to create DLP compositor for: %1").arg(command));
        return -1;
    }

    // Forward compositor logs through the Qt message handler.
    coder_dlp_set_log_callback(comp, &DlpCompositorManager::onCompositorLog, this);

    // Apply current DLP policy to the new compositor.
    coder_dlp_set_policy(comp, &m_currentPolicy);

    // Apply watermark identity if set.
    if (!m_watermarkIdentity.isEmpty()) {
        coder_dlp_set_watermark_identity(comp, m_watermarkIdentity.constData());
    }

    // Set the compositor window title so it's identifiable in the taskbar.
    const QString title =
        QStringLiteral("Coder Secure - %1").arg(appName.isEmpty() ? command : appName);
    const QByteArray titleUtf8 = title.toUtf8();
    coder_dlp_set_output_title(comp, titleUtf8.constData());

    // Integrate the wlroots event loop with Qt via QSocketNotifier.
    // Do NOT dispatch on a separate thread — wlroots is not thread-safe.
    const int fd = coder_dlp_get_fd(comp);
    auto notifier = std::make_unique<QSocketNotifier>(fd, QSocketNotifier::Read, this);
    connect(notifier.get(), &QSocketNotifier::activated, this,
            [comp]() { coder_dlp_dispatch(comp); });

    // Build sandbox config.
    const QByteArray cmdUtf8 = command.toUtf8();
    const QByteArray wsPathUtf8 = workspacePath.toUtf8();

    // Convert extra bind paths to C strings.
    QVector<QByteArray> bindPathsUtf8;
    bindPathsUtf8.reserve(extraBindPaths.size());
    QVector<const char*> bindPathPtrs;
    bindPathPtrs.reserve(extraBindPaths.size());
    for (const auto& path : extraBindPaths) {
        bindPathsUtf8.append(path.toUtf8());
        bindPathPtrs.append(bindPathsUtf8.last().constData());
    }

    coder_dlp_sandbox_config sandbox{};
    sandbox.workspace_path = workspacePath.isEmpty() ? nullptr : wsPathUtf8.constData();
    sandbox.network_namespace = isolateNetwork ? "dlp_net" : nullptr;
    sandbox.isolate_pid = isolatePid;
    sandbox.isolate_ipc = isolateIpc;
    sandbox.isolate_filesystem = isolateFilesystem;
    sandbox.bind_home_rw = bindHomeRw;
    sandbox.extra_bind_paths =
        bindPathPtrs.isEmpty() ? nullptr : const_cast<const char**>(bindPathPtrs.data());
    sandbox.extra_bind_count = bindPathPtrs.size();
    sandbox.filter_dbus = filterDbus;

    // Convert QStringList → C string array for the sandbox config
    QList<QByteArray> talkNameBytes;
    QList<const char*> talkNamePtrs;
    for (const auto& name : dbusAllowedNames) {
        talkNameBytes.append(name.toUtf8());
        talkNamePtrs.append(talkNameBytes.last().constData());
    }
    sandbox.dbus_talk_names =
        talkNamePtrs.isEmpty() ? nullptr : const_cast<const char**>(talkNamePtrs.data());
    sandbox.dbus_talk_count = talkNamePtrs.size();

    const int pid = coder_dlp_launch_app(comp, cmdUtf8.constData(), &sandbox);
    if (pid < 0) {
        emit errorOccurred(QStringLiteral("Failed to launch app: %1").arg(command));
        notifier.reset();
        coder_dlp_destroy(comp);
        return -1;
    }

    const bool wasEmpty = m_apps.empty();

    // Store the running app entry.
    auto app = std::make_unique<RunningApp>();
    app->pid = pid;
    app->appName = appName.isEmpty() ? command : appName;
    app->command = command;
    app->compositor = comp;
    app->notifier = std::move(notifier);
    m_apps.push_back(std::move(app));

    m_runningAppModel->addApp(pid, m_apps.back()->appName, command);

    emit appCountChanged();
    if (wasEmpty) {
        emit runningChanged();
    }

    return pid;
}

void DlpCompositorManager::stopApp(int pid) {
    for (int i = 0; i < static_cast<int>(m_apps.size()); ++i) {
        if (m_apps[i]->pid == pid) {
            kill(pid, SIGTERM);
            removeApp(i);
            return;
        }
    }
}

void DlpCompositorManager::stopAll() {
    // Send SIGTERM to all tracked processes.
    for (const auto& app : m_apps) {
        if (app->pid > 0) {
            kill(app->pid, SIGTERM);
        }
    }

    // Destroy compositors and notifiers via RAII.
    for (const auto& app : m_apps) {
        app->notifier.reset();
        if (app->compositor) {
            coder_dlp_destroy(app->compositor);
            app->compositor = nullptr;
        }
    }

    const bool wasRunning = !m_apps.empty();
    m_apps.clear();
    m_runningAppModel->clear();

    if (wasRunning) {
        emit appCountChanged();
        emit runningChanged();
    }
}

void DlpCompositorManager::updatePolicy(bool clipboardBlockOutgoing, bool clipboardBlockIncoming,
                                        bool screenshotBlock, bool fileSandbox, bool networkSandbox,
                                        bool watermarkEnabled) {
    m_currentPolicy.clipboard_block_outgoing = clipboardBlockOutgoing;
    m_currentPolicy.clipboard_block_incoming = clipboardBlockIncoming;
    m_currentPolicy.screenshot_block = screenshotBlock;
    m_currentPolicy.file_sandbox = fileSandbox;
    m_currentPolicy.network_sandbox = networkSandbox;
    m_currentPolicy.watermark_enabled = watermarkEnabled;

    // Apply to all active compositors.
    for (const auto& app : m_apps) {
        if (app->compositor) {
            coder_dlp_set_policy(app->compositor, &m_currentPolicy);
        }
    }
}

void DlpCompositorManager::setWatermarkIdentity(const QString& identity) {
    m_watermarkIdentity = identity.toUtf8();

    // Apply to all active compositors.
    for (const auto& app : m_apps) {
        if (app->compositor) {
            coder_dlp_set_watermark_identity(app->compositor, m_watermarkIdentity.constData());
        }
    }
}

void DlpCompositorManager::checkChildProcesses() {
    // Iterate in reverse so removal doesn't invalidate remaining indices.
    for (int i = static_cast<int>(m_apps.size()) - 1; i >= 0; --i) {
        int status = 0;
        const pid_t result = waitpid(m_apps[i]->pid, &status, WNOHANG);
        if (result > 0) {
            // Log exit status.  Don't remove yet — Wayland clients may still be connected.
            if (WIFEXITED(status)) {
                qCInfo(lcDlpMgr) << "Child pid" << result << "exited with status"
                                 << WEXITSTATUS(status);
            } else if (WIFSIGNALED(status)) {
                qCInfo(lcDlpMgr) << "Child pid" << result << "killed by signal" << WTERMSIG(status);
            }
            m_apps[i]->bwrapExited = true;
        }

        // Clean up only when bwrap has exited AND no Wayland clients remain.
        if (m_apps[i]->bwrapExited && coder_dlp_get_client_count(m_apps[i]->compositor) == 0) {
            qCInfo(lcDlpMgr) << "Compositor has no clients, cleaning up pid" << m_apps[i]->pid;
            removeApp(i);
        }
    }
}

void DlpCompositorManager::removeApp(int index) {
    if (index < 0 || index >= static_cast<int>(m_apps.size())) {
        return;
    }

    const int pid = m_apps[index]->pid;

    // Destroy compositor and notifier (RAII handles notifier).
    m_apps[index]->notifier.reset();
    if (m_apps[index]->compositor) {
        coder_dlp_destroy(m_apps[index]->compositor);
        m_apps[index]->compositor = nullptr;
    }

    m_apps.erase(m_apps.begin() + index);
    m_runningAppModel->removeApp(pid);

    emit appCountChanged();
    if (m_apps.empty()) {
        emit runningChanged();
    }
}

// static
void DlpCompositorManager::onCompositorLog(const char* message, void* /*data*/) {
    // Forward through the Qt message handler so compositor logs appear in the
    // log file alongside Qt messages.
    QByteArray msg(message);
    if (msg.startsWith("[wlr ERROR]")) {
        qCCritical(lcDlpMgr).noquote() << msg.mid(12);
    } else if (msg.startsWith("[wlr INFO]")) {
        qCInfo(lcDlpMgr).noquote() << msg.mid(11);
    } else if (msg.startsWith("[wlr DEBUG]")) {
        qCDebug(lcDlpMgr).noquote() << msg.mid(12);
    } else {
        qCDebug(lcDlpMgr).noquote() << msg;
    }
}
