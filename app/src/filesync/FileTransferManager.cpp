#include "filesync/FileTransferManager.h"

#include <QFileInfo>
#include <QLoggingCategory>
#include <QRegularExpression>

#include <algorithm>

Q_LOGGING_CATEGORY(lcFileTransfer, "coder.filesync")

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

FileTransferManager::FileTransferManager(QObject* parent) : QAbstractListModel(parent) {
    // Periodically purge old finished transfers so the list doesn't grow forever.
    connect(&m_cleanupTimer, &QTimer::timeout, this,
            &FileTransferManager::purgeOldFinishedTransfers);
    m_cleanupTimer.start(kCleanupIntervalMs);
}

FileTransferManager::~FileTransferManager() {
    // Terminate any still-running SCP processes.
    for (auto& t : m_transfers) {
        if (t->process && t->process->state() != QProcess::NotRunning) {
            t->process->kill();
            t->process->waitForFinished(500);
        }
    }
}

// ---------------------------------------------------------------------------
// QAbstractListModel overrides
// ---------------------------------------------------------------------------

int FileTransferManager::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) return 0;
    return static_cast<int>(m_transfers.size());
}

QVariant FileTransferManager::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 ||
        index.row() >= static_cast<int>(m_transfers.size())) {
        return {};
    }

    const auto& info = m_transfers[index.row()]->info;
    switch (role) {
        case TransferIdRole:
            return info.id;
        case LocalPathRole:
            return info.localPath;
        case RemotePathRole:
            return info.remotePath;
        case IsUploadRole:
            return info.isUpload;
        case ProgressRole:
            return info.progress();
        case StateRole:
            return static_cast<int>(info.state);
        case ErrorMessageRole:
            return info.errorMessage;
        case AgentHostnameRole:
            return info.agentHostname;
        default:
            return {};
    }
}

QHash<int, QByteArray> FileTransferManager::roleNames() const {
    return {
        {TransferIdRole, "transferId"},     {LocalPathRole, "localPath"},
        {RemotePathRole, "remotePath"},     {IsUploadRole, "isUpload"},
        {ProgressRole, "progress"},         {StateRole, "state"},
        {ErrorMessageRole, "errorMessage"}, {AgentHostnameRole, "agentHostname"},
    };
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

int FileTransferManager::download(const QString& agentHostname, const QString& remotePath,
                                  const QString& localPath) {
    const int id = m_nextId++;

    auto transfer = std::make_unique<RunningTransfer>();
    transfer->info.id = id;
    transfer->info.agentHostname = agentHostname;
    transfer->info.localPath = localPath;
    transfer->info.remotePath = remotePath;
    transfer->info.isUpload = false;
    transfer->info.state = FileTransferState::Queued;

    const int previousActive = activeTransferCount();
    const int row = static_cast<int>(m_transfers.size());
    beginInsertRows(QModelIndex(), row, row);
    m_transfers.push_back(std::move(transfer));
    endInsertRows();

    qCInfo(lcFileTransfer) << "Queued download" << id << "from" << agentHostname << ":"
                           << remotePath << "→" << localPath;

    startScpProcess(*m_transfers.back());
    emitActiveCountIfChanged(previousActive);
    return id;
}

int FileTransferManager::upload(const QString& agentHostname, const QString& localPath,
                                const QString& remotePath) {
    const int id = m_nextId++;

    auto transfer = std::make_unique<RunningTransfer>();
    transfer->info.id = id;
    transfer->info.agentHostname = agentHostname;
    transfer->info.localPath = localPath;
    transfer->info.remotePath = remotePath;
    transfer->info.isUpload = true;
    transfer->info.state = FileTransferState::Queued;

    // For uploads, try to determine total size from the local file.
    const QFileInfo fi(localPath);
    if (fi.exists() && fi.isFile()) {
        transfer->info.bytesTotal = fi.size();
    }

    const int previousActive = activeTransferCount();
    const int row = static_cast<int>(m_transfers.size());
    beginInsertRows(QModelIndex(), row, row);
    m_transfers.push_back(std::move(transfer));
    endInsertRows();

    qCInfo(lcFileTransfer) << "Queued upload" << id << "from" << localPath << "→" << agentHostname
                           << ":" << remotePath;

    startScpProcess(*m_transfers.back());
    emitActiveCountIfChanged(previousActive);
    return id;
}

void FileTransferManager::cancelTransfer(int transferId) {
    auto* t = findTransfer(transferId);
    if (!t) {
        qCWarning(lcFileTransfer) << "cancelTransfer: unknown id" << transferId;
        return;
    }

    if (t->info.state == FileTransferState::Completed ||
        t->info.state == FileTransferState::Failed ||
        t->info.state == FileTransferState::Cancelled) {
        return;  // Already terminal.
    }

    const int previousActive = activeTransferCount();
    t->info.state = FileTransferState::Cancelled;

    if (t->process && t->process->state() != QProcess::NotRunning) {
        // Try graceful termination first; force-kill after a short timeout.
        t->process->terminate();
        if (!t->process->waitForFinished(1000)) {
            t->process->kill();
            t->process->waitForFinished(500);
        }
    }

    const int row = rowForTransfer(transferId);
    if (row >= 0) {
        emit dataChanged(index(row), index(row), {StateRole});
    }

    qCInfo(lcFileTransfer) << "Cancelled transfer" << transferId;
    emit transferCancelled(transferId);
    emitActiveCountIfChanged(previousActive);
}

int FileTransferManager::activeTransferCount() const {
    return static_cast<int>(
        std::count_if(m_transfers.begin(), m_transfers.end(), [](const auto& t) {
            return t->info.state == FileTransferState::Queued ||
                   t->info.state == FileTransferState::Running;
        }));
}

QList<FileTransfer> FileTransferManager::transfers() const {
    QList<FileTransfer> result;
    result.reserve(static_cast<int>(m_transfers.size()));
    for (const auto& t : m_transfers) {
        result.append(t->info);
    }
    return result;
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

void FileTransferManager::startScpProcess(RunningTransfer& transfer) {
    transfer.process = std::make_unique<QProcess>();

    // Build SCP command.
    // -o StrictHostKeyChecking=no  — VPN tunnel is already authenticated.
    // -o UserKnownHostsFile=/dev/null — Don't pollute known_hosts with tailnet IPs.
    // -o BatchMode=no — Allow progress output.
    QStringList args;
    args << QStringLiteral("-o") << QStringLiteral("StrictHostKeyChecking=no");
    args << QStringLiteral("-o") << QStringLiteral("UserKnownHostsFile=/dev/null");
    args << QStringLiteral("-o") << QStringLiteral("LogLevel=ERROR");

    const QString remoteSpec =
        QStringLiteral("%1:%2").arg(transfer.info.agentHostname, transfer.info.remotePath);

    if (transfer.info.isUpload) {
        args << transfer.info.localPath << remoteSpec;
    } else {
        args << remoteSpec << transfer.info.localPath;
    }

    const int id = transfer.info.id;
    QProcess* proc = transfer.process.get();  // non-owning, for lambdas

    // Wire stderr for progress parsing.
    connect(proc, &QProcess::readyReadStandardError, this,
            [this, id, proc]() { parseScpProgress(id, proc->readAllStandardError()); });

    // Wire process finish.
    connect(
        proc, &QProcess::finished, this, [this, id](int exitCode, QProcess::ExitStatus exitStatus) {
            auto* t = findTransfer(id);
            if (!t) return;

            const int previousActive = activeTransferCount();

            // If already cancelled, don't overwrite the state.
            if (t->info.state == FileTransferState::Cancelled) return;

            const int row = rowForTransfer(id);

            if (exitStatus == QProcess::CrashExit || exitCode != 0) {
                t->info.state = FileTransferState::Failed;
                t->info.errorMessage = QStringLiteral("scp exited with code %1").arg(exitCode);

                // Append any remaining stderr as context.
                const QByteArray remaining = t->process->readAllStandardError();
                if (!remaining.isEmpty()) {
                    t->info.errorMessage +=
                        QStringLiteral(": %1").arg(QString::fromUtf8(remaining).trimmed());
                }

                if (row >= 0) {
                    emit dataChanged(index(row), index(row), {StateRole, ErrorMessageRole});
                }

                qCWarning(lcFileTransfer) << "Transfer" << id << "failed:" << t->info.errorMessage;
                emit transferFailed(id, t->info.errorMessage);
            } else {
                t->info.state = FileTransferState::Completed;
                t->info.bytesTransferred = t->info.bytesTotal;

                if (row >= 0) {
                    emit dataChanged(index(row), index(row), {StateRole, ProgressRole});
                }

                qCInfo(lcFileTransfer) << "Transfer" << id << "completed";
                emit transferCompleted(id);
            }

            emitActiveCountIfChanged(previousActive);
        });

    // Wire error handler (process failed to start).
    connect(proc, &QProcess::errorOccurred, this, [this, id](QProcess::ProcessError error) {
        auto* t = findTransfer(id);
        if (!t) return;
        if (t->info.state == FileTransferState::Cancelled) return;

        // QProcess::finished is NOT emitted when the process fails to start,
        // so we must handle it here.
        if (error == QProcess::FailedToStart) {
            const int previousActive = activeTransferCount();
            t->info.state = FileTransferState::Failed;
            t->info.errorMessage = QStringLiteral("Failed to start scp process");

            const int row = rowForTransfer(id);
            if (row >= 0) {
                emit dataChanged(index(row), index(row), {StateRole, ErrorMessageRole});
            }

            qCWarning(lcFileTransfer) << "Transfer" << id << "failed to start scp";
            emit transferFailed(id, t->info.errorMessage);
            emitActiveCountIfChanged(previousActive);
        }
    });

    // Launch.
    transfer.info.state = FileTransferState::Running;
    const int row = rowForTransfer(transfer.info.id);
    if (row >= 0) {
        emit dataChanged(index(row), index(row), {StateRole});
    }

    qCDebug(lcFileTransfer) << "Starting scp" << args.join(QLatin1Char(' '));
    proc->start(QStringLiteral("scp"), args);
    emit transferStarted(transfer.info.id);
}

void FileTransferManager::parseScpProgress(int transferId, const QByteArray& output) {
    auto* t = findTransfer(transferId);
    if (!t) return;

    t->stderrBuffer.append(output);

    // SCP progress lines are terminated by '\r' or '\n'.  Process all complete
    // lines; keep partial data in the buffer for the next read.
    while (true) {
        int sep = t->stderrBuffer.indexOf('\r');
        if (sep < 0) sep = t->stderrBuffer.indexOf('\n');
        if (sep < 0) break;

        const QByteArray line = t->stderrBuffer.left(sep).trimmed();
        t->stderrBuffer.remove(0, sep + 1);

        if (line.isEmpty()) continue;

        // Match SCP progress: "filename  45%  1234KB  500.0KB/s  00:05"
        // We capture the percentage and the size token.
        static const QRegularExpression progressRe(
            QStringLiteral(R"(\s+(\d{1,3})%\s+(\d+(?:\.\d+)?)(KB|MB|GB|TB|B)\s)"));

        const QString lineStr = QString::fromUtf8(line);
        const auto match = progressRe.match(lineStr);
        if (!match.hasMatch()) continue;

        const int percent = match.captured(1).toInt();
        const double sizeVal = match.captured(2).toDouble();
        const QString sizeUnit = match.captured(3);

        // Convert transferred size to bytes.
        qint64 transferred = 0;
        if (sizeUnit == QLatin1String("B")) {
            transferred = static_cast<qint64>(sizeVal);
        } else if (sizeUnit == QLatin1String("KB")) {
            transferred = static_cast<qint64>(sizeVal * 1024);
        } else if (sizeUnit == QLatin1String("MB")) {
            transferred = static_cast<qint64>(sizeVal * 1024 * 1024);
        } else if (sizeUnit == QLatin1String("GB")) {
            transferred = static_cast<qint64>(sizeVal * 1024 * 1024 * 1024);
        } else if (sizeUnit == QLatin1String("TB")) {
            transferred = static_cast<qint64>(sizeVal * 1024.0 * 1024 * 1024 * 1024);
        }

        // If we have a percentage but no total yet, estimate it.
        if (percent > 0 && t->info.bytesTotal == 0 && transferred > 0) {
            t->info.bytesTotal = transferred * 100 / percent;
        }

        t->info.bytesTransferred = transferred;

        const int row = rowForTransfer(transferId);
        if (row >= 0) {
            emit dataChanged(index(row), index(row), {ProgressRole});
        }

        emit transferProgress(transferId, t->info.bytesTransferred, t->info.bytesTotal);
    }
}

FileTransferManager::RunningTransfer* FileTransferManager::findTransfer(int transferId) {
    auto it = std::find_if(m_transfers.begin(), m_transfers.end(),
                           [transferId](const auto& t) { return t->info.id == transferId; });
    return (it != m_transfers.end()) ? it->get() : nullptr;
}

const FileTransferManager::RunningTransfer* FileTransferManager::findTransfer(
    int transferId) const {
    auto it = std::find_if(m_transfers.cbegin(), m_transfers.cend(),
                           [transferId](const auto& t) { return t->info.id == transferId; });
    return (it != m_transfers.cend()) ? it->get() : nullptr;
}

int FileTransferManager::rowForTransfer(int transferId) const {
    for (int i = 0; i < static_cast<int>(m_transfers.size()); ++i) {
        if (m_transfers[i]->info.id == transferId) return i;
    }
    return -1;
}

void FileTransferManager::purgeOldFinishedTransfers() {
    // Count finished (non-active) transfers.
    int finishedCount = 0;
    for (const auto& t : m_transfers) {
        if (t->info.state != FileTransferState::Queued &&
            t->info.state != FileTransferState::Running) {
            ++finishedCount;
        }
    }

    if (finishedCount <= kMaxFinishedTransfers) return;

    const int toRemove = finishedCount - kMaxFinishedTransfers;
    int removed = 0;

    for (auto it = m_transfers.begin(); it != m_transfers.end() && removed < toRemove;) {
        const auto state = (*it)->info.state;
        if (state != FileTransferState::Queued && state != FileTransferState::Running) {
            const int row = static_cast<int>(std::distance(m_transfers.begin(), it));
            beginRemoveRows(QModelIndex(), row, row);
            it = m_transfers.erase(it);
            endRemoveRows();
            ++removed;
        } else {
            ++it;
        }
    }

    if (removed > 0) {
        qCDebug(lcFileTransfer) << "Purged" << removed << "old finished transfers";
        emit transferCountChanged();
    }
}

void FileTransferManager::emitActiveCountIfChanged(int previousCount) {
    if (activeTransferCount() != previousCount) {
        emit transferCountChanged();
    }
}
