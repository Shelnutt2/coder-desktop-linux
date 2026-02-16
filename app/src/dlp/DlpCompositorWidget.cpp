#include "dlp/DlpCompositorWidget.h"

#include <QByteArray>

DlpCompositorWidget::DlpCompositorWidget(QObject *parent)
    : QObject(parent)
{
}

DlpCompositorWidget::~DlpCompositorWidget()
{
    stop();
}

bool DlpCompositorWidget::isAvailable() const
{
    return coder_dlp_is_available();
}

bool DlpCompositorWidget::isRunning() const
{
    return m_compositor != nullptr;
}

int DlpCompositorWidget::launchedAppCount() const
{
    return m_appCount;
}

bool DlpCompositorWidget::start()
{
    if (m_compositor) {
        return true;  // already running
    }

    if (!isAvailable()) {
        emit errorOccurred(QStringLiteral("DLP compositor requires a Wayland session"));
        return false;
    }

    m_compositor = coder_dlp_create(nullptr);
    if (!m_compositor) {
        emit errorOccurred(QStringLiteral("Failed to create DLP compositor"));
        return false;
    }

    // Integrate the wlroots event loop with Qt via QSocketNotifier on the
    // compositor's file descriptor.  Do NOT dispatch on a separate thread —
    // wlroots is not thread-safe.
    const int fd = coder_dlp_get_fd(m_compositor);
    m_notifier = std::make_unique<QSocketNotifier>(fd, QSocketNotifier::Read, this);
    connect(m_notifier.get(), &QSocketNotifier::activated, this, [this]() {
        if (m_compositor) {
            coder_dlp_dispatch(m_compositor);
        }
    });

    // Register the surface callback so QML gets notified of new windows.
    coder_dlp_on_new_surface(m_compositor, &DlpCompositorWidget::onNewSurface, this);

    emit runningChanged();
    return true;
}

void DlpCompositorWidget::stop()
{
    if (!m_compositor) {
        return;
    }

    m_notifier.reset();
    coder_dlp_destroy(m_compositor);
    m_compositor = nullptr;

    const int oldCount = m_appCount;
    m_appCount = 0;

    emit runningChanged();
    if (oldCount != 0) {
        emit appCountChanged();
    }
}

int DlpCompositorWidget::launchApp(const QString &command,
                                    const QString &workspacePath,
                                    bool isolatePid,
                                    bool isolateIpc,
                                    bool isolateNetwork)
{
    if (!m_compositor) {
        emit errorOccurred(QStringLiteral("DLP compositor is not running"));
        return -1;
    }

    const QByteArray cmdUtf8 = command.toUtf8();

    coder_dlp_sandbox_config sandbox{};
    const QByteArray wsPathUtf8 = workspacePath.toUtf8();
    sandbox.workspace_path = workspacePath.isEmpty() ? nullptr : wsPathUtf8.constData();
    sandbox.network_namespace = isolateNetwork ? "dlp_net" : nullptr;
    sandbox.isolate_pid = isolatePid;
    sandbox.isolate_ipc = isolateIpc;

    const int pid = coder_dlp_launch_app(m_compositor, cmdUtf8.constData(), &sandbox);
    if (pid < 0) {
        emit errorOccurred(QStringLiteral("Failed to launch app: %1").arg(command));
        return -1;
    }

    ++m_appCount;
    emit appCountChanged();
    return pid;
}

void DlpCompositorWidget::updatePolicy(bool clipboardBlockOutgoing,
                                        bool clipboardBlockIncoming,
                                        bool screenshotBlock,
                                        bool fileSandbox,
                                        bool networkSandbox)
{
    if (!m_compositor) {
        return;
    }

    coder_dlp_policy policy{};
    policy.clipboard_block_outgoing = clipboardBlockOutgoing;
    policy.clipboard_block_incoming = clipboardBlockIncoming;
    policy.screenshot_block = screenshotBlock;
    policy.file_sandbox = fileSandbox;
    policy.network_sandbox = networkSandbox;

    coder_dlp_set_policy(m_compositor, &policy);
}

// static
void DlpCompositorWidget::onNewSurface(coder_dlp_compositor * /*comp*/,
                                        void * /*surface*/,
                                        void *data)
{
    auto *self = static_cast<DlpCompositorWidget *>(data);
    // The callback may fire from within coder_dlp_dispatch() which runs on the
    // Qt main thread (via QSocketNotifier), so a direct emit is safe here.
    emit self->newSurface();
}
