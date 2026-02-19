#ifndef FILETRANSFERMANAGER_H
#define FILETRANSFERMANAGER_H

#include <QAbstractListModel>
#include <QObject>
#include <QProcess>
#include <QString>
#include <QTimer>
#include <memory>
#include <vector>

/// Describes the current state of a single file transfer.
enum class FileTransferState {
    Queued,
    Running,
    Completed,
    Failed,
    Cancelled,
};
Q_DECLARE_METATYPE(FileTransferState)

/// Snapshot of a single file transfer (upload or download via SCP).
struct FileTransfer {
    int id = 0;
    QString agentHostname;
    QString localPath;
    QString remotePath;
    bool isUpload = false;  ///< true = local→remote, false = remote→local
    qint64 bytesTransferred = 0;
    qint64 bytesTotal = 0;
    FileTransferState state = FileTransferState::Queued;
    QString errorMessage;

    /// Progress in [0.0, 1.0]. Returns 0 when total is unknown.
    [[nodiscard]] double progress() const {
        return (bytesTotal > 0) ? static_cast<double>(bytesTransferred) / bytesTotal : 0.0;
    }
};

/// Manages individual file uploads/downloads to Coder workspaces via SCP.
///
/// Workspace agents are reachable over the VPN tunnel at their tailnet hostname
/// on port 22 (SSH).  This class wraps `scp` subprocess calls with progress
/// tracking and cancellation.
///
/// All operations run on the Qt main thread event loop — no background threads.
///
/// Usage from QML:
/// @code
///   var id = fileTransferManager.upload("myworkspace.coder", "/tmp/a.txt", "/home/coder/a.txt");
///   // listen to transferProgress / transferCompleted / transferFailed signals
///   fileTransferManager.cancelTransfer(id);
/// @endcode
class FileTransferManager : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int activeTransferCount READ activeTransferCount NOTIFY transferCountChanged)
    /// Total number of transfers (for reactive QML bindings).
    Q_PROPERTY(int count READ rowCount NOTIFY transferCountChanged)

public:
    /// Model roles exposed to QML delegates.
    enum Role {
        TransferIdRole = Qt::UserRole + 1,
        LocalPathRole,
        RemotePathRole,
        IsUploadRole,
        ProgressRole,  ///< double in [0.0, 1.0]
        StateRole,     ///< FileTransferState as int
        ErrorMessageRole,
        AgentHostnameRole,
    };
    Q_ENUM(Role)

    explicit FileTransferManager(QObject* parent = nullptr);
    ~FileTransferManager() override;

    // -- QAbstractListModel overrides ----------------------------------------

    [[nodiscard]] int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    // -- Public API ----------------------------------------------------------

    /// Start a download: `scp agent:remotePath localPath`.
    /// @return Transfer ID for tracking.
    Q_INVOKABLE int download(const QString& agentHostname, const QString& remotePath,
                             const QString& localPath);

    /// Start an upload: `scp localPath agent:remotePath`.
    /// @return Transfer ID for tracking.
    Q_INVOKABLE int upload(const QString& agentHostname, const QString& localPath,
                           const QString& remotePath);

    /// Cancel a running or queued transfer.
    Q_INVOKABLE void cancelTransfer(int transferId);

    /// Number of transfers in Queued or Running state.
    [[nodiscard]] int activeTransferCount() const;

    /// Snapshot of all tracked transfers (including finished ones).
    [[nodiscard]] QList<FileTransfer> transfers() const;

signals:
    void transferStarted(int id);
    void transferProgress(int id, qint64 bytesTransferred, qint64 bytesTotal);
    void transferCompleted(int id);
    void transferFailed(int id, const QString& errorMessage);
    void transferCancelled(int id);
    void transferCountChanged();

private:
    /// Internal bookkeeping for a transfer and its SCP subprocess.
    struct RunningTransfer {
        FileTransfer info;
        std::unique_ptr<QProcess> process;
        QByteArray stderrBuffer;  ///< Accumulates partial stderr lines for parsing.
    };

    /// Maximum number of finished transfers to keep for UI display.
    static constexpr int kMaxFinishedTransfers = 50;

    /// Delay before purging old finished transfers (ms).
    static constexpr int kCleanupIntervalMs = 30'000;

    int m_nextId = 1;
    std::vector<std::unique_ptr<RunningTransfer>> m_transfers;
    QTimer m_cleanupTimer;

    // -- Helpers -------------------------------------------------------------

    /// Build SCP arguments and launch the QProcess for @p transfer.
    void startScpProcess(RunningTransfer& transfer);

    /// Parse SCP progress output (stderr) for the given transfer.
    ///
    /// SCP emits lines like:
    ///   `filename  45%  1234KB  500.0KB/s  00:05`
    /// We extract the percentage and (optionally) absolute bytes.
    void parseScpProgress(int transferId, const QByteArray& output);

    /// Find a RunningTransfer by id (or nullptr).
    [[nodiscard]] RunningTransfer* findTransfer(int transferId);
    [[nodiscard]] const RunningTransfer* findTransfer(int transferId) const;

    /// Row index for a given transfer id, or -1.
    [[nodiscard]] int rowForTransfer(int transferId) const;

    /// Remove finished transfers that exceed kMaxFinishedTransfers.
    void purgeOldFinishedTransfers();

    /// Notify the model + property that the active count may have changed.
    void emitActiveCountIfChanged(int previousCount);
};

#endif  // FILETRANSFERMANAGER_H
