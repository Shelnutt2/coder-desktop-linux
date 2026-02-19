#ifndef FILESYNCMANAGER_H
#define FILESYNCMANAGER_H

#include <QAbstractListModel>
#include <QObject>
#include <QProcess>
#include <QString>
#include <QTimer>
#include <vector>

#include "filesync/FileSyncSession.h"

class MutagenDaemon;
class SettingsManager;

/// Manages Mutagen sync sessions and exposes them as a QAbstractListModel for QML.
///
/// Uses the Mutagen CLI (`mutagen sync` sub-commands) rather than gRPC to avoid
/// a heavy protobuf/gRPC dependency.  All CLI calls include
/// `--data-directory=<daemon dataDir>` so they target the same daemon instance
/// managed by MutagenDaemon.
///
/// Session state is refreshed every 2 seconds via `mutagen sync list --json`.
///
/// Policy enforcement:
/// - If both upload and download are disabled, the feature is unavailable.
/// - If only upload is disabled, sessions are created in download-only mode.
/// - If only download is disabled, sessions are created in upload-only mode.
class FileSyncManager : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(bool available READ isAvailable NOTIFY availableChanged)
    Q_PROPERTY(bool uploadAllowed READ uploadAllowed NOTIFY policyChanged)
    Q_PROPERTY(bool downloadAllowed READ downloadAllowed NOTIFY policyChanged)
    Q_PROPERTY(int sessionCount READ sessionCount NOTIFY sessionCountChanged)
    Q_PROPERTY(QString statusSummary READ statusSummary NOTIFY statusSummaryChanged)

public:
    /// Model roles exposed to QML delegates.
    enum Roles {
        SessionIdRole = Qt::UserRole + 1,
        LocalPathRole,
        WorkspaceRole,
        RemotePathRole,
        StatusRole,
        StatusStringRole,
        StatusCategoryRole,
        SizeBytesRole,
        ConflictCountRole,
        PausedRole,
        ModeRole,
    };
    Q_ENUM(Roles)

    /// @param settings  Non-owning; must outlive this object.
    /// @param daemon    Non-owning; must outlive this object.
    explicit FileSyncManager(SettingsManager* settings, MutagenDaemon* daemon,
                             QObject* parent = nullptr);
    ~FileSyncManager() override;

    // Non-copyable, non-movable (QObject).
    FileSyncManager(const FileSyncManager&) = delete;
    FileSyncManager& operator=(const FileSyncManager&) = delete;

    // -- QAbstractListModel overrides ----------------------------------------

    [[nodiscard]] int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    // -- Session CRUD (QML-callable) -----------------------------------------

    /// Create a new sync session.  Starts the daemon lazily if needed.
    Q_INVOKABLE void createSession(const QString& localPath, const QString& workspace,
                                   const QString& remotePath);

    /// Pause a running session.
    Q_INVOKABLE void pauseSession(const QString& sessionId);

    /// Resume a paused session.
    Q_INVOKABLE void resumeSession(const QString& sessionId);

    /// Reset a session (clear conflicts / re-scan).
    Q_INVOKABLE void resetSession(const QString& sessionId);

    /// Terminate (delete) a session.  Auto-stops the daemon if none remain.
    Q_INVOKABLE void terminateSession(const QString& sessionId);

    // -- Properties ----------------------------------------------------------

    /// True when file-sync is available (VPN connected and not fully policy-blocked).
    [[nodiscard]] bool isAvailable() const;

    /// True when uploads are allowed by DLP policy.
    [[nodiscard]] bool uploadAllowed() const;

    /// True when downloads are allowed by DLP policy.
    [[nodiscard]] bool downloadAllowed() const;

    /// Number of tracked sessions.
    [[nodiscard]] int sessionCount() const;

    /// Human-readable summary for the system tray, e.g. "2 sessions (synced)".
    [[nodiscard]] QString statusSummary() const;

    /// Set VPN connection state (called from outside, e.g. VpnBridge).
    void setVpnConnected(bool connected);

signals:
    void availableChanged();
    void policyChanged();
    void sessionCountChanged();
    void statusSummaryChanged();
    void errorOccurred(const QString& message);

private:
    /// Interval between `mutagen sync list` polls (ms).
    static constexpr int kPollIntervalMs = 2000;

    /// Timeout for individual CLI commands (ms).
    static constexpr int kCommandTimeoutMs = 15000;

    // -- Helpers -------------------------------------------------------------

    /// Build common CLI arguments: `--data-directory=<dataDir>`.
    [[nodiscard]] QStringList dataDirArgs() const;

    /// Run a short-lived `mutagen` CLI command and return its stdout.
    /// Blocks the calling thread (via QProcess::waitForFinished).
    /// On failure, emits errorOccurred() and returns an empty QByteArray.
    [[nodiscard]] QByteArray runMutagenSync(const QStringList& args);

    /// Parse JSON output from `mutagen sync list --json` into session list.
    [[nodiscard]] std::vector<FileSyncSession> parseSessionList(const QByteArray& json) const;

    /// Parse a single status string from Mutagen JSON into FileSyncStatus.
    [[nodiscard]] static FileSyncStatus parseStatus(const QString& statusStr);

    /// Refresh the session list via `mutagen sync list --json`.
    void refreshSessions();

    /// Replace the model contents and emit change signals as needed.
    void updateModel(std::vector<FileSyncSession> newSessions);

    /// Determine the sync mode based on current DLP policy.
    [[nodiscard]] FileSyncMode effectiveSyncMode() const;

    SettingsManager* m_settings;  // non-owning
    MutagenDaemon* m_daemon;      // non-owning
    QTimer m_pollTimer;
    bool m_vpnConnected = false;

    std::vector<FileSyncSession> m_sessions;
};

#endif  // FILESYNCMANAGER_H
