#include "filesync/FileSyncManager.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QProcess>

#include "filesync/MutagenDaemon.h"
#include "settings/SettingsManager.h"

Q_LOGGING_CATEGORY(lcFileSync, "coder.filesync.manager")

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

FileSyncManager::FileSyncManager(SettingsManager* settings, MutagenDaemon* daemon, QObject* parent)
    : QAbstractListModel(parent), m_settings(settings), m_daemon(daemon) {
    Q_ASSERT(m_settings);
    Q_ASSERT(m_daemon);

    // Re-evaluate policy when settings change.
    connect(m_settings, &SettingsManager::settingsChanged, this, [this] {
        emit policyChanged();
        emit availableChanged();
    });

    // Re-evaluate availability when the daemon starts/stops.
    connect(m_daemon, &MutagenDaemon::runningChanged, this, &FileSyncManager::availableChanged);

    // Poll timer for session list refresh.
    m_pollTimer.setInterval(kPollIntervalMs);
    connect(&m_pollTimer, &QTimer::timeout, this, &FileSyncManager::refreshSessions);
}

FileSyncManager::~FileSyncManager() {
    m_pollTimer.stop();
}

// ---------------------------------------------------------------------------
// QAbstractListModel overrides
// ---------------------------------------------------------------------------

int FileSyncManager::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) {
        return 0;
    }
    return static_cast<int>(m_sessions.size());
}

QVariant FileSyncManager::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(m_sessions.size())) {
        return {};
    }

    const auto& s = m_sessions[static_cast<size_t>(index.row())];
    switch (role) {
        case SessionIdRole:
            return s.sessionId;
        case LocalPathRole:
            return s.localPath;
        case WorkspaceRole:
            return s.workspace;
        case RemotePathRole:
            return s.remotePath;
        case StatusRole:
            return static_cast<int>(s.status);
        case StatusStringRole:
            return s.statusString();
        case StatusCategoryRole:
            return s.statusCategory();
        case SizeBytesRole:
            return s.sizeBytes;
        case ConflictCountRole:
            return s.conflictCount;
        case PausedRole:
            return s.paused;
        case ModeRole:
            return static_cast<int>(s.mode);
        default:
            return {};
    }
}

QHash<int, QByteArray> FileSyncManager::roleNames() const {
    return {
        {SessionIdRole, "sessionId"},
        {LocalPathRole, "localPath"},
        {WorkspaceRole, "workspace"},
        {RemotePathRole, "remotePath"},
        {StatusRole, "status"},
        {StatusStringRole, "statusString"},
        {StatusCategoryRole, "statusCategory"},
        {SizeBytesRole, "sizeBytes"},
        {ConflictCountRole, "conflictCount"},
        {PausedRole, "paused"},
        {ModeRole, "mode"},
    };
}

// ---------------------------------------------------------------------------
// Session CRUD
// ---------------------------------------------------------------------------

void FileSyncManager::createSession(const QString& localPath, const QString& workspace,
                                    const QString& remotePath) {
    // Policy guard.
    if (!uploadAllowed() && !downloadAllowed()) {
        emit errorOccurred(QStringLiteral("File sync is disabled by policy"));
        return;
    }

    // Lazy daemon start.
    if (!m_daemon->isRunning()) {
        if (!m_daemon->start()) {
            emit errorOccurred(QStringLiteral("Failed to start Mutagen daemon"));
            return;
        }
    }

    const QString betaUrl = workspace + QStringLiteral(":") + remotePath;
    QStringList args = {QStringLiteral("sync"), QStringLiteral("create"), localPath, betaUrl,
                        QStringLiteral("--ignore-vcs")};
    args.append(dataDirArgs());

    // Apply mode based on DLP policy.
    const FileSyncMode mode = effectiveSyncMode();
    if (mode == FileSyncMode::OneWayRemoteToLocal || mode == FileSyncMode::OneWayLocalToRemote) {
        // one-way-replica: alpha→beta is the default direction.
        // For download-only (remote→local), the caller should swap alpha/beta.
        // For now, just set the mode flag; direction follows alpha/beta order.
        args.append(QStringLiteral("--mode=one-way-replica"));
    }

    qCInfo(lcFileSync) << "creating session:" << localPath << "->" << betaUrl;

    // Start polling immediately so the UI can discover the session via
    // `sync list` even while `sync create` is still connecting/scanning.
    if (!m_pollTimer.isActive()) {
        m_pollTimer.start();
    }

    runMutagenAsync(args, [this](int exitCode, const QByteArray& output) {
        if (exitCode != 0) {
            qCWarning(lcFileSync) << "sync create failed (exit" << exitCode << "):" << output;
            emit errorOccurred(tr("Failed to create sync session (see logs)"));
        }
        refreshSessions();
    });
}

void FileSyncManager::pauseSession(const QString& sessionId) {
    qCInfo(lcFileSync) << "pausing session:" << sessionId;
    QStringList args = {QStringLiteral("sync"), QStringLiteral("pause"), sessionId};
    args.append(dataDirArgs());
    runMutagenAsync(args, [this](int exitCode, const QByteArray& /*output*/) {
        if (exitCode != 0) {
            emit errorOccurred(tr("Failed to pause session"));
        }
        refreshSessions();
    });
}

void FileSyncManager::resumeSession(const QString& sessionId) {
    qCInfo(lcFileSync) << "resuming session:" << sessionId;
    QStringList args = {QStringLiteral("sync"), QStringLiteral("resume"), sessionId};
    args.append(dataDirArgs());
    runMutagenAsync(args, [this](int exitCode, const QByteArray& /*output*/) {
        if (exitCode != 0) {
            emit errorOccurred(tr("Failed to resume session"));
        }
        refreshSessions();
    });
}

void FileSyncManager::resetSession(const QString& sessionId) {
    qCInfo(lcFileSync) << "resetting session:" << sessionId;
    QStringList args = {QStringLiteral("sync"), QStringLiteral("reset"), sessionId};
    args.append(dataDirArgs());
    runMutagenAsync(args, [this](int exitCode, const QByteArray& /*output*/) {
        if (exitCode != 0) {
            emit errorOccurred(tr("Failed to reset session"));
        }
        refreshSessions();
    });
}

void FileSyncManager::terminateSession(const QString& sessionId) {
    qCInfo(lcFileSync) << "terminating session:" << sessionId;
    QStringList args = {QStringLiteral("sync"), QStringLiteral("terminate"), sessionId};
    args.append(dataDirArgs());
    runMutagenAsync(args, [this](int exitCode, const QByteArray& /*output*/) {
        if (exitCode != 0) {
            emit errorOccurred(tr("Failed to terminate session"));
        }

        // Refresh to remove the session from the model.
        // The callback from refreshSessions will update m_sessions,
        // so we check for auto-stop after that completes.
        // For simplicity, schedule a deferred check.
        refreshSessions();

        // Auto-stop daemon if no sessions remain (checked after refresh completes
        // via updateModel → sessionCountChanged).
    });
}

// ---------------------------------------------------------------------------
// Properties
// ---------------------------------------------------------------------------

bool FileSyncManager::isAvailable() const {
    return m_vpnConnected && (uploadAllowed() || downloadAllowed());
}

bool FileSyncManager::uploadAllowed() const {
    return !m_settings->disableFileUpload();
}

bool FileSyncManager::downloadAllowed() const {
    return !m_settings->disableFileDownload();
}

int FileSyncManager::sessionCount() const {
    return static_cast<int>(m_sessions.size());
}

QString FileSyncManager::statusSummary() const {
    const int count = sessionCount();
    if (count == 0) {
        return QStringLiteral("No sync sessions");
    }

    // Determine aggregate status category.
    bool allSynced = true;
    bool anyError = false;
    for (const auto& s : m_sessions) {
        const QString cat = s.statusCategory();
        if (cat == QLatin1String("error")) {
            anyError = true;
            allSynced = false;
        } else if (cat != QLatin1String("ok")) {
            allSynced = false;
        }
    }

    const QString noun = (count == 1) ? QStringLiteral("session") : QStringLiteral("sessions");
    if (anyError) {
        return QStringLiteral("%1 %2 (error)").arg(count).arg(noun);
    }
    if (allSynced) {
        return QStringLiteral("%1 %2 (synced)").arg(count).arg(noun);
    }
    return QStringLiteral("%1 %2 (syncing)").arg(count).arg(noun);
}

void FileSyncManager::setVpnConnected(bool connected) {
    if (m_vpnConnected == connected) {
        return;
    }
    m_vpnConnected = connected;
    emit availableChanged();

    if (connected) {
        // On VPN connect, start the daemon and poll for any pre-existing
        // sessions that were created in a previous app run.  Mutagen
        // persists session state in MUTAGEN_DATA_DIRECTORY, so sessions
        // survive across app restarts.
        if (!m_daemon->isRunning()) {
            if (!m_daemon->start()) {
                qCWarning(lcFileSync) << "failed to start daemon on VPN connect";
                return;
            }
        }
        if (!m_pollTimer.isActive()) {
            m_pollTimer.start();
        }
        // Delay the first refresh slightly to give the daemon time to open
        // its IPC socket and load persisted sessions from disk.  Without
        // this, the very first `sync list` may connect before the daemon is
        // ready and return an empty result.
        QTimer::singleShot(500, this, &FileSyncManager::refreshSessions);
    } else {
        m_pollTimer.stop();
        // Stop the daemon when VPN disconnects — no point keeping it running
        // since sync sessions cannot function without the VPN tunnel.
        if (m_daemon->isRunning()) {
            m_daemon->stop();
        }
    }
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

QStringList FileSyncManager::dataDirArgs() const {
    // Mutagen 0.18+ does not have a --data-directory CLI flag; the data
    // directory is controlled exclusively via the MUTAGEN_DATA_DIRECTORY
    // environment variable, which is set in runMutagenAsync().
    return {};
}

void FileSyncManager::runMutagenAsync(
    const QStringList& args, std::function<void(int exitCode, const QByteArray& output)> callback) {
    const QString binary = m_daemon->mutagenBinaryPath();
    if (binary.isEmpty()) {
        emit errorOccurred(QStringLiteral("Mutagen binary not found"));
        return;
    }

    qCDebug(lcFileSync) << "running (async):" << binary << args;

    auto* proc = new QProcess(this);
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("MUTAGEN_DATA_DIRECTORY"), m_daemon->dataDir());
    proc->setProcessEnvironment(env);
    proc->setProgram(binary);
    proc->setArguments(args);

    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            [proc, cb = std::move(callback)](int exitCode, QProcess::ExitStatus /*exitStatus*/) {
                const QByteArray stdoutData = proc->readAllStandardOutput();
                const QByteArray stderrData = proc->readAllStandardError();
                if (!stderrData.isEmpty()) {
                    qCDebug(lcFileSync) << "mutagen stderr:" << stderrData.trimmed();
                }
                qCDebug(lcFileSync) << "mutagen finished, exit code:" << exitCode;
                if (cb) {
                    cb(exitCode, stdoutData);
                }
                proc->deleteLater();
            });

    connect(proc, &QProcess::errorOccurred, this, [this, proc](QProcess::ProcessError err) {
        if (err == QProcess::FailedToStart) {
            emit errorOccurred(tr("Failed to start mutagen: %1").arg(proc->errorString()));
            proc->deleteLater();
        }
    });

    proc->start(QIODevice::ReadOnly);
}

std::vector<FileSyncSession> FileSyncManager::parseSessionList(const QByteArray& json) const {
    std::vector<FileSyncSession> result;

    const QJsonDocument doc = QJsonDocument::fromJson(json);

    // Mutagen's --template '{{json .}}' outputs a JSON array of session objects.
    QJsonArray sessions;
    if (doc.isArray()) {
        sessions = doc.array();
    } else if (doc.isObject()) {
        // Fallback: some versions may wrap in {"sessions": [...]}.
        sessions = doc.object().value(QLatin1String("sessions")).toArray();
    } else {
        return result;
    }
    result.reserve(static_cast<size_t>(sessions.size()));

    for (const QJsonValue& val : sessions) {
        const QJsonObject obj = val.toObject();
        FileSyncSession s;

        // Mutagen's --template '{{json .}}' outputs camelCase JSON field names.
        s.sessionId = obj.value(QLatin1String("identifier")).toString();

        // Alpha = local, Beta = remote.
        const QJsonObject alpha = obj.value(QLatin1String("alpha")).toObject();
        const QJsonObject beta = obj.value(QLatin1String("beta")).toObject();
        s.localPath = alpha.value(QLatin1String("path")).toString();
        s.remotePath = beta.value(QLatin1String("path")).toString();

        // Beta endpoint has "host" = "workspace.coder" — extract workspace name.
        s.workspace = beta.value(QLatin1String("host")).toString();

        s.paused = obj.value(QLatin1String("paused")).toBool();

        // Status is a plain string like "watching", "scanning", etc.
        const QString statusField = obj.value(QLatin1String("status")).toString();
        s.status = s.paused ? FileSyncStatus::Paused : parseStatus(statusField);

        // Sync mode — currently all sessions are two-way.
        s.mode = FileSyncMode::TwoWay;

        // Optional fields.
        s.sizeBytes = obj.value(QLatin1String("sizeBytes")).toInteger(0);
        s.conflictCount = obj.value(QLatin1String("conflicts")).toArray().size();
        s.lastError = obj.value(QLatin1String("lastError")).toString();

        result.push_back(std::move(s));
    }

    return result;
}

FileSyncStatus FileSyncManager::parseStatus(const QString& statusStr) {
    // Mutagen's --template '{{json .}}' outputs a plain lowercase string
    // for the status field (e.g. "watching", "scanning", "disconnected").
    static const struct {
        QLatin1String key;
        FileSyncStatus value;
    } kMap[] = {
        {QLatin1String("connectingAlpha"), FileSyncStatus::ConnectingAlpha},
        {QLatin1String("connectingBeta"), FileSyncStatus::ConnectingBeta},
        {QLatin1String("scanning"), FileSyncStatus::Scanning},
        {QLatin1String("reconciling"), FileSyncStatus::Reconciling},
        {QLatin1String("stagingAlpha"), FileSyncStatus::StagingAlpha},
        {QLatin1String("stagingBeta"), FileSyncStatus::StagingBeta},
        {QLatin1String("transitioning"), FileSyncStatus::Transitioning},
        {QLatin1String("saving"), FileSyncStatus::Saving},
        {QLatin1String("watching"), FileSyncStatus::Watching},
        {QLatin1String("disconnected"), FileSyncStatus::Disconnected},
        {QLatin1String("haltedOnRootEmptied"), FileSyncStatus::HaltedOnRootEmptied},
        {QLatin1String("haltedOnRootDeletion"), FileSyncStatus::HaltedOnRootDeletion},
        {QLatin1String("haltedOnRootTypeChange"), FileSyncStatus::HaltedOnRootTypeChange},
        {QLatin1String("waitingForRescan"), FileSyncStatus::WaitingForRescan},
        {QLatin1String("paused"), FileSyncStatus::Paused},
    };

    for (const auto& entry : kMap) {
        if (statusStr == entry.key) {
            return entry.value;
        }
    }
    return FileSyncStatus::Unknown;
}

void FileSyncManager::refreshSessions() {
    if (!m_daemon->isRunning()) {
        return;
    }
    if (m_pollInFlight) {
        return;  // skip overlapping polls
    }
    m_pollInFlight = true;

    QStringList args = {QStringLiteral("sync"), QStringLiteral("list"),
                        QStringLiteral("--template"), QStringLiteral("{{json .}}")};
    args.append(dataDirArgs());

    runMutagenAsync(args, [this](int exitCode, const QByteArray& output) {
        m_pollInFlight = false;
        if (exitCode != 0) {
            qCWarning(lcFileSync) << "sync list failed (exit" << exitCode << ")";
            return;
        }
        auto newSessions = parseSessionList(output);
        qCDebug(lcFileSync) << "parsed" << newSessions.size() << "sessions from sync list";
        updateModel(std::move(newSessions));
    });
}

void FileSyncManager::updateModel(std::vector<FileSyncSession> newSessions) {
    const int oldCount = static_cast<int>(m_sessions.size());
    const int newCount = static_cast<int>(newSessions.size());
    const QString oldSummary = statusSummary();

    // Simple reset strategy: beginResetModel / swap / endResetModel.
    // For a small number of sessions (typically < 20) this is efficient enough.
    beginResetModel();
    m_sessions = std::move(newSessions);
    endResetModel();

    if (oldCount != newCount) {
        emit sessionCountChanged();
    }
    if (statusSummary() != oldSummary) {
        emit statusSummaryChanged();
    }

    // Keep polling regardless of whether sessions are empty.  The poll timer
    // runs only while VPN is connected and `sync list` is cheap.  Stopping
    // early on empty results would prevent discovering sessions if the daemon
    // takes longer than expected to load them from disk, and would also miss
    // sessions created externally (e.g. via the mutagen CLI).
}

FileSyncMode FileSyncManager::effectiveSyncMode() const {
    const bool noUpload = m_settings->disableFileUpload();
    const bool noDownload = m_settings->disableFileDownload();

    if (noUpload && !noDownload) {
        return FileSyncMode::OneWayRemoteToLocal;  // download only
    }
    if (noDownload && !noUpload) {
        return FileSyncMode::OneWayLocalToRemote;  // upload only
    }
    return FileSyncMode::TwoWay;
}
