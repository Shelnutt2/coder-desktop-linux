#include "filesync/MutagenDaemon.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QProcess>
#include <QStandardPaths>

Q_LOGGING_CATEGORY(lcMutagen, "coder.filesync.daemon")

namespace {
/// Timeout (ms) for the graceful `mutagen daemon stop` during shutdown.
constexpr int kGracefulStopTimeoutMs = 5000;

/// Timeout (ms) for the orphan-cleanup `mutagen daemon stop` during start.
constexpr int kOrphanCleanupTimeoutMs = 3000;
}  // namespace

MutagenDaemon::MutagenDaemon(QObject* parent) : QObject(parent) {
    // Data directory: ~/.local/share/coder-desktop/mutagen/
    const QString dataRoot = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    m_dataDir = dataRoot + QStringLiteral("/coder-desktop/mutagen");

    m_binaryPath = findMutagenBinary();
    if (m_binaryPath.isEmpty()) {
        qCWarning(lcMutagen) << "mutagen binary not found in any search path";
    } else {
        qCInfo(lcMutagen) << "mutagen binary:" << m_binaryPath;
    }
}

MutagenDaemon::~MutagenDaemon() {
    stop();
}

bool MutagenDaemon::start() {
    if (m_process && m_process->state() != QProcess::NotRunning) {
        qCDebug(lcMutagen) << "daemon already running";
        return true;
    }

    if (m_binaryPath.isEmpty()) {
        emit errorOccurred(QStringLiteral("mutagen binary not found"));
        return false;
    }

    // Ensure the data directory exists.
    QDir().mkpath(m_dataDir);

    // Clean up any orphaned daemon from a previous crash.
    cleanupOrphan();

    m_intentionalStop = false;
    m_process = std::make_unique<QProcess>(this);

    // Set environment for the daemon subprocess.
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("MUTAGEN_DATA_DIRECTORY"), m_dataDir);
    // Avoid user SSH config conflicts — the Coder VPN handles transport.
    env.insert(QStringLiteral("MUTAGEN_SSH_CONFIG_PATH"), QStringLiteral("none"));
    m_process->setProcessEnvironment(env);

    connect(m_process.get(), &QProcess::finished, this, &MutagenDaemon::onProcessFinished);
    connect(m_process.get(), &QProcess::errorOccurred, this, &MutagenDaemon::onProcessError);

    qCInfo(lcMutagen) << "starting daemon:" << m_binaryPath << "daemon run";
    m_process->start(m_binaryPath, {QStringLiteral("daemon"), QStringLiteral("run")});

    if (!m_process->waitForStarted(kGracefulStopTimeoutMs)) {
        qCWarning(lcMutagen) << "daemon failed to start:" << m_process->errorString();
        emit errorOccurred(
            QStringLiteral("Failed to start mutagen daemon: %1").arg(m_process->errorString()));
        m_process.reset();
        return false;
    }

    qCInfo(lcMutagen) << "daemon started, pid:" << m_process->processId();
    emit runningChanged();
    return true;
}

void MutagenDaemon::stop() {
    if (!m_process || m_process->state() == QProcess::NotRunning) {
        return;
    }

    m_intentionalStop = true;

    // Try graceful shutdown via `mutagen daemon stop`.
    if (!m_binaryPath.isEmpty()) {
        qCInfo(lcMutagen) << "requesting graceful daemon stop";
        QProcess stopProc;
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert(QStringLiteral("MUTAGEN_DATA_DIRECTORY"), m_dataDir);
        stopProc.setProcessEnvironment(env);
        stopProc.start(m_binaryPath, {QStringLiteral("daemon"), QStringLiteral("stop")});

        if (stopProc.waitForFinished(kGracefulStopTimeoutMs)) {
            // Give the main daemon process a moment to exit.
            if (m_process && m_process->waitForFinished(kGracefulStopTimeoutMs)) {
                qCInfo(lcMutagen) << "daemon stopped gracefully";
                return;
            }
        }
    }

    // Graceful stop failed or timed out — forcefully kill.
    if (m_process && m_process->state() != QProcess::NotRunning) {
        qCWarning(lcMutagen) << "graceful stop timed out, killing daemon";
        m_process->kill();
        m_process->waitForFinished(kGracefulStopTimeoutMs);
    }
}

bool MutagenDaemon::isRunning() const {
    return m_process && m_process->state() != QProcess::NotRunning;
}

QString MutagenDaemon::socketPath() const {
    return m_dataDir + QStringLiteral("/daemon/daemon.sock");
}

QString MutagenDaemon::mutagenBinaryPath() const {
    return m_binaryPath;
}

// --- Private ----------------------------------------------------------------

void MutagenDaemon::cleanupOrphan() {
    if (m_binaryPath.isEmpty()) {
        return;
    }

    qCDebug(lcMutagen) << "cleaning up potential orphan daemon";
    QProcess stopProc;
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("MUTAGEN_DATA_DIRECTORY"), m_dataDir);
    stopProc.setProcessEnvironment(env);
    stopProc.start(m_binaryPath, {QStringLiteral("daemon"), QStringLiteral("stop")});
    stopProc.waitForFinished(kOrphanCleanupTimeoutMs);
    // Ignore exit code — orphan may not exist.
}

void MutagenDaemon::onProcessFinished(int exitCode, QProcess::ExitStatus status) {
    qCInfo(lcMutagen) << "daemon exited, code:" << exitCode
                      << "status:" << (status == QProcess::CrashExit ? "crash" : "normal");

    emit runningChanged();

    if (!m_intentionalStop) {
        const QString msg =
            (status == QProcess::CrashExit)
                ? QStringLiteral("Mutagen daemon crashed (exit code %1)").arg(exitCode)
                : QStringLiteral("Mutagen daemon exited unexpectedly (exit code %1)").arg(exitCode);
        emit errorOccurred(msg);
    }
}

void MutagenDaemon::onProcessError(QProcess::ProcessError error) {
    qCWarning(lcMutagen) << "daemon process error:" << error;

    if (m_process) {
        emit errorOccurred(
            QStringLiteral("Mutagen daemon error: %1").arg(m_process->errorString()));
    }
}

QString MutagenDaemon::findMutagenBinary() const {
    const QString binaryName = QStringLiteral("mutagen");

    // 1. Same directory as the application executable.
    const QString appDir = QCoreApplication::applicationDirPath();
    const QString sameDir = appDir + QStringLiteral("/") + binaryName;
    if (QFileInfo::exists(sameDir)) {
        return QFileInfo(sameDir).canonicalFilePath();
    }

    // 2. Relative lib directory (typical installed layout: bin/../lib/coder-desktop/).
    const QString libDir = appDir + QStringLiteral("/../lib/coder-desktop/") + binaryName;
    if (QFileInfo::exists(libDir)) {
        return QFileInfo(libDir).canonicalFilePath();
    }

    // 3. $PATH lookup.
    const QString inPath = QStandardPaths::findExecutable(binaryName);
    if (!inPath.isEmpty()) {
        return inPath;
    }

    return {};
}
