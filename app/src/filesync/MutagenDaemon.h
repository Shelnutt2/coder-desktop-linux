#ifndef MUTAGENDAEMON_H
#define MUTAGENDAEMON_H

#include <QObject>
#include <QProcess>
#include <QString>
#include <memory>

/// Manages the Mutagen daemon subprocess lifecycle.
///
/// The daemon runs in foreground mode (`mutagen daemon run`) so that QProcess
/// can manage its lifetime.  Communication with the daemon happens over a Unix
/// domain socket at socketPath(); the gRPC client is handled separately.
///
/// On start(), any orphaned daemon from a previous crash is cleaned up first.
/// On stop() (and in the destructor), a graceful shutdown is attempted before
/// falling back to SIGKILL.
class MutagenDaemon : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool running READ isRunning NOTIFY runningChanged)
    Q_PROPERTY(QString socketPath READ socketPath CONSTANT)

public:
    explicit MutagenDaemon(QObject* parent = nullptr);
    ~MutagenDaemon() override;

    // Non-copyable, non-movable (QObject).
    MutagenDaemon(const MutagenDaemon&) = delete;
    MutagenDaemon& operator=(const MutagenDaemon&) = delete;

    /// Start the daemon.  Returns true if started successfully or already running.
    [[nodiscard]] bool start();

    /// Stop the daemon gracefully, then forcefully if needed.
    void stop();

    /// Whether the daemon process is currently running.
    [[nodiscard]] bool isRunning() const;

    /// Path to the Unix domain socket used by the daemon for gRPC.
    [[nodiscard]] QString socketPath() const;

    /// Resolved path to the mutagen binary, or empty if not found.
    [[nodiscard]] QString mutagenBinaryPath() const;

signals:
    void runningChanged();
    void errorOccurred(const QString& message);

private:
    /// Kill any leftover daemon from a prior crash.
    void cleanupOrphan();

    /// Slot: daemon process exited.
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);

    /// Slot: daemon process encountered an error (e.g. failed to start).
    void onProcessError(QProcess::ProcessError error);

    /// Search for the mutagen binary in well-known locations and $PATH.
    [[nodiscard]] QString findMutagenBinary() const;

    std::unique_ptr<QProcess> m_process;
    QString m_dataDir;     // ~/.local/share/coder-desktop/mutagen/
    QString m_binaryPath;  // resolved path to mutagen binary
    bool m_intentionalStop = false;
};

#endif  // MUTAGENDAEMON_H
