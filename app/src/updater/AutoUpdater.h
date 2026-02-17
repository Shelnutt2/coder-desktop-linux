#ifndef AUTOUPDATER_H
#define AUTOUPDATER_H

#include <QObject>
#include <QString>
#include <QTimer>

#include <memory>

class QNetworkAccessManager;
class QNetworkReply;
class SettingsManager;

/// Checks GitHub Releases for newer versions of Coder Desktop.
///
/// On construction (and optionally on a periodic timer), queries the GitHub
/// Releases API.  If the latest release tag is newer than the compiled-in
/// APP_VERSION, emits updateAvailable().
///
/// This class does NOT download or install anything — it only notifies.
class AutoUpdater : public QObject {
    Q_OBJECT

    Q_PROPERTY(QString latestVersion READ latestVersion NOTIFY updateAvailable)
    Q_PROPERTY(QString downloadUrl READ downloadUrl NOTIFY updateAvailable)
    Q_PROPERTY(bool updateReady READ updateReady NOTIFY updateAvailable)

public:
    /// @param currentVersion  The compiled-in application version (e.g. "0.1.0").
    /// @param settingsManager  Non-owning pointer; used to read the checkForUpdates setting.
    /// @param parent  QObject parent.
    explicit AutoUpdater(const QString& currentVersion,
                         SettingsManager* settingsManager,  // non-owning
                         QObject* parent = nullptr);

    ~AutoUpdater() override = default;

    // Non-copyable, non-movable (QObject).
    AutoUpdater(const AutoUpdater&) = delete;
    AutoUpdater& operator=(const AutoUpdater&) = delete;

    /// The latest version string from GitHub (empty until first successful check).
    [[nodiscard]] QString latestVersion() const;

    /// The browser_download_url for the first asset of the latest release (empty until check).
    [[nodiscard]] QString downloadUrl() const;

    /// True if a newer version was detected.
    [[nodiscard]] bool updateReady() const;

    /// Trigger a check now.  No-op if checkForUpdates setting is false.
    Q_INVOKABLE void checkNow();

    /// Set the periodic check interval.  Pass 0 to disable periodic checks.
    void setCheckInterval(std::chrono::milliseconds interval);

signals:
    /// Emitted when a newer version is found.
    void updateAvailable(const QString& version, const QString& downloadUrl);

    /// Emitted when a check completes with no update (or on error).
    void checkFinished();

private slots:
    void onReplyFinished(QNetworkReply* reply);

private:
    /// Compare two semver strings.  Returns true if remote > local.
    [[nodiscard]] static bool isNewerVersion(const QString& local, const QString& remote);

    /// Parse a version tag like "v0.2.0" or "0.2.0" into (major, minor, patch).
    /// Returns false on parse failure.
    [[nodiscard]] static bool parseSemver(const QString& tag, int& major, int& minor, int& patch);

    QString m_currentVersion;
    QString m_latestVersion;
    QString m_downloadUrl;
    bool m_updateReady = false;

    SettingsManager* m_settings = nullptr;  // non-owning
    std::unique_ptr<QNetworkAccessManager> m_nam;
    QTimer m_timer;
};

#endif  // AUTOUPDATER_H
