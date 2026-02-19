#ifndef FILESYNCSESSION_H
#define FILESYNCSESSION_H

#include <QObject>
#include <QString>

/// Status of a Mutagen sync session.
///
/// These map to the states reported by the Mutagen daemon over gRPC.
/// Grouped by category: working, ok, error, and user-initiated.
enum class FileSyncStatus {
    Unknown,

    // Working states — sync is actively progressing.
    ConnectingAlpha,
    ConnectingBeta,
    Scanning,
    Reconciling,
    StagingAlpha,
    StagingBeta,
    Transitioning,
    Saving,

    // OK — sync is idle and healthy.
    Watching,

    // Error states — sync has stopped due to a problem.
    Disconnected,
    HaltedOnRootEmptied,
    HaltedOnRootDeletion,
    HaltedOnRootTypeChange,
    WaitingForRescan,

    // User-initiated states.
    Paused,
};

/// Sync direction mode.
///
/// The default is bidirectional.  DLP policy may restrict to one-way
/// to enforce file-upload or file-download restrictions.
enum class FileSyncMode {
    TwoWay,               /// Bidirectional (default).
    OneWayLocalToRemote,  /// Upload only (disableFileDownload).
    OneWayRemoteToLocal,  /// Download only (disableFileUpload).
};

/// Represents a single Mutagen sync session.
///
/// This is a plain data struct populated from the gRPC response.
/// The UI binds to the model that owns a list of these.
struct FileSyncSession {
    QString sessionId;   /// Mutagen's internal session identifier.
    QString localPath;   /// Alpha (local) directory.
    QString workspace;   /// Workspace/agent hostname.
    QString remotePath;  /// Beta (remote) directory.

    FileSyncStatus status = FileSyncStatus::Unknown;
    FileSyncMode mode = FileSyncMode::TwoWay;

    qint64 sizeBytes = 0;   /// Total synced size in bytes.
    int conflictCount = 0;  /// Number of unresolved conflicts.
    bool paused = false;
    QString lastError;  /// Last error message, if any.

    /// Human-readable status string for the UI.
    [[nodiscard]] QString statusString() const;

    /// Status category for color coding: "ok", "working", "error", or "paused".
    [[nodiscard]] QString statusCategory() const;
};

Q_DECLARE_METATYPE(FileSyncSession)

#endif  // FILESYNCSESSION_H
