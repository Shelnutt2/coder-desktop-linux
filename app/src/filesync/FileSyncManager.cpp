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
    if (mode == FileSyncMode::OneWayRemoteToLocal) {
        // Download only: beta → alpha (remote is source).
        args.append(QStringLiteral("--mode=one-way-replica"));
        args.append(QStringLiteral("--from=beta"));
    } else if (mode == FileSyncMode::OneWayLocalToRemote) {
        // Upload only: alpha → beta (local is source).
        args.append(QStringLiteral("--mode=one-way-replica"));
        args.append(QStringLiteral("--from=alpha"));
    }

    qCInfo(lcFileSync) << "creating session:" << localPath << "->" << betaUrl;
    const QByteArray output = runMutagenSync(args);
    if (output.isNull()) {
        return;  // error already emitted
    }

    // Start polling if not already running.
    if (!m_pollTimer.isActive()) {
        m_pollTimer.start();
    }

    // Trigger an immediate refresh to pick up the new session.
    refreshSessions();
}

void FileSyncManager::pauseSession(const QString& sessionId) {
    qCInfo(lcFileSync) << "pausing session:" << sessionId;
    QStringList args = {QStringLiteral("sync"), QStringLiteral("pause"), sessionId};
    args.append(dataDirArgs());
    (void)runMutagenSync(args);
    refreshSessions();
}

void FileSyncManager::resumeSession(const QString& sessionId) {
    qCInfo(lcFileSync) << "resuming session:" << sessionId;
    QStringList args = {QStringLiteral("sync"), QStringLiteral("resume"), sessionId};
    args.append(dataDirArgs());
    (void)runMutagenSync(args);
    refreshSessions();
}

void FileSyncManager::resetSession(const QString& sessionId) {
    qCInfo(lcFileSync) << "resetting session:" << sessionId;
    QStringList args = {QStringLiteral("sync"), QStringLiteral("reset"), sessionId};
    args.append(dataDirArgs());
    (void)runMutagenSync(args);
    refreshSessions();
}

void FileSyncManager::terminateSession(const QString& sessionId) {
    qCInfo(lcFileSync) << "terminating session:" << sessionId;
    QStringList args = {QStringLiteral("sync"), QStringLiteral("terminate"), sessionId};
    args.append(dataDirArgs());
    (void)runMutagenSync(args);

    // Refresh to remove the session from the model.
    refreshSessions();

    // Auto-stop daemon if no sessions remain.
    if (m_sessions.empty() && m_daemon->isRunning()) {
        qCInfo(lcFileSync) << "no sessions remain, stopping daemon";
        m_pollTimer.stop();
        m_daemon->stop();
    }
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

    // Start/stop polling based on connectivity.
    if (connected && !m_sessions.empty()) {
        if (!m_pollTimer.isActive()) {
            m_pollTimer.start();
        }
    } else if (!connected) {
        m_pollTimer.stop();
    }
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

QStringList FileSyncManager::dataDirArgs() const {
    return {QStringLiteral("--data-directory=") + m_daemon->dataDir()};
}

QByteArray FileSyncManager::runMutagenSync(const QStringList& args) {
    const QString binary = m_daemon->mutagenBinaryPath();
    if (binary.isEmpty()) {
        emit errorOccurred(QStringLiteral("Mutagen binary not found"));
        return {};
    }

    qCDebug(lcFileSync) << "running:" << binary << args;

    QProcess proc;
    proc.start(binary, args);
    if (!proc.waitForFinished(kCommandTimeoutMs)) {
        const QString msg = QStringLiteral("Mutagen command timed out: mutagen %1")
                                .arg(args.join(QLatin1Char(' ')));
        qCWarning(lcFileSync) << msg;
        proc.kill();
        proc.waitForFinished(3000);
        emit errorOccurred(msg);
        return {};
    }

    if (proc.exitCode() != 0) {
        const QString stderr = QString::fromUtf8(proc.readAllStandardError()).trimmed();
        const QString msg =
            QStringLiteral("Mutagen command failed (exit %1): %2").arg(proc.exitCode()).arg(stderr);
        qCWarning(lcFileSync) << msg;
        emit errorOccurred(msg);
        return {};
    }

    return proc.readAllStandardOutput();
}

std::vector<FileSyncSession> FileSyncManager::parseSessionList(const QByteArray& json) const {
    std::vector<FileSyncSession> result;

    const QJsonDocument doc = QJsonDocument::fromJson(json);
    if (!doc.isObject()) {
        return result;
    }

    // Mutagen JSON: { "sessions": [ { ... }, ... ] }
    const QJsonArray sessions = doc.object().value(QLatin1String("sessions")).toArray();
    result.reserve(static_cast<size_t>(sessions.size()));

    for (const QJsonValue& val : sessions) {
        const QJsonObject obj = val.toObject();
        FileSyncSession s;
        s.sessionId = obj.value(QLatin1String("identifier")).toString();

        // Alpha = local, Beta = remote.
        const QJsonObject alpha = obj.value(QLatin1String("alpha")).toObject();
        const QJsonObject beta = obj.value(QLatin1String("beta")).toObject();
        s.localPath = alpha.value(QLatin1String("path")).toString();
        s.remotePath = beta.value(QLatin1String("path")).toString();

        // Beta URL is "workspace:path" — extract workspace from the URL.
        const QString betaUrl = beta.value(QLatin1String("url")).toString();
        const int colonIdx = betaUrl.indexOf(QLatin1Char(':'));
        if (colonIdx > 0) {
            s.workspace = betaUrl.left(colonIdx);
        }

        s.paused = obj.value(QLatin1String("paused")).toBool();
        s.status = s.paused ? FileSyncStatus::Paused
                            : parseStatus(obj.value(QLatin1String("status")).toString());

        // Sync mode.
        const QString modeStr = obj.value(QLatin1String("mode")).toString();
        if (modeStr == QLatin1String("one-way-replica")) {
            // Check the "from" field to distinguish upload vs download.
            const QString from = obj.value(QLatin1String("from")).toString();
            if (from == QLatin1String("beta")) {
                s.mode = FileSyncMode::OneWayRemoteToLocal;
            } else {
                s.mode = FileSyncMode::OneWayLocalToRemote;
            }
        } else {
            s.mode = FileSyncMode::TwoWay;
        }

        // Optional fields.
        s.sizeBytes = obj.value(QLatin1String("sizeBytes")).toInteger(0);
        s.conflictCount = obj.value(QLatin1String("conflictCount")).toInt(0);
        s.lastError = obj.value(QLatin1String("lastError")).toString();

        result.push_back(std::move(s));
    }

    return result;
}

FileSyncStatus FileSyncManager::parseStatus(const QString& statusStr) {
    // Map Mutagen's status strings to our enum.
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

    QStringList args = {QStringLiteral("sync"), QStringLiteral("list"), QStringLiteral("--json")};
    args.append(dataDirArgs());

    const QByteArray output = runMutagenSync(args);
    if (output.isNull()) {
        return;  // error already emitted
    }

    auto newSessions = parseSessionList(output);
    updateModel(std::move(newSessions));
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
