#ifndef SETTINGSMANAGER_H
#define SETTINGSMANAGER_H

#include <QObject>
#include <QSettings>
#include <QString>
#include <QStringList>
#include <QVariant>

#include <memory>

#include "settings/MdmConfigManager.h"

/// Three-layer settings manager for Coder Desktop.
///
/// Resolution order (highest priority first):
///   1. MDM policy  — `/etc/coder-desktop/policy.json`
///   2. User prefs  — `~/.config/coder-desktop/settings.json` (via QSettings)
///   3. Compiled defaults
///
/// When MDM is not present every setting is user-editable.
/// When MDM locks a setting, setUserPreference() becomes a no-op for that key.
class SettingsManager : public QObject {
    Q_OBJECT

public:
    /// Where a setting's current effective value comes from.
    enum class Source { Default = 0, User = 1, Mdm = 2 };
    Q_ENUM(Source)

    // -- Q_PROPERTY declarations ------------------------------------------

    // Deployment
    Q_PROPERTY(QString deploymentUrl READ deploymentUrl NOTIFY settingsChanged)
    Q_PROPERTY(bool deploymentUrlLocked READ deploymentUrlLocked NOTIFY settingsChanged)

    Q_PROPERTY(QStringList allowedDeployments READ allowedDeployments NOTIFY settingsChanged)
    Q_PROPERTY(bool allowedDeploymentsLocked READ allowedDeploymentsLocked NOTIFY settingsChanged)

    Q_PROPERTY(bool disableMultiDeployment READ disableMultiDeployment NOTIFY settingsChanged)
    Q_PROPERTY(bool disableMultiDeploymentLocked READ disableMultiDeploymentLocked NOTIFY settingsChanged)

    // VPN
    Q_PROPERTY(bool requireVpn READ requireVpn NOTIFY settingsChanged)
    Q_PROPERTY(bool requireVpnLocked READ requireVpnLocked NOTIFY settingsChanged)

    Q_PROPERTY(bool autoConnectVpn READ autoConnectVpn NOTIFY settingsChanged)
    Q_PROPERTY(bool autoConnectVpnLocked READ autoConnectVpnLocked NOTIFY settingsChanged)

    // DLP
    Q_PROPERTY(bool dlpEnabled READ dlpEnabled NOTIFY settingsChanged)
    Q_PROPERTY(bool dlpEnabledLocked READ dlpEnabledLocked NOTIFY settingsChanged)

    Q_PROPERTY(bool dlpClipboardBlock READ dlpClipboardBlock NOTIFY settingsChanged)
    Q_PROPERTY(bool dlpClipboardBlockLocked READ dlpClipboardBlockLocked NOTIFY settingsChanged)

    Q_PROPERTY(bool dlpScreenshotBlock READ dlpScreenshotBlock NOTIFY settingsChanged)
    Q_PROPERTY(bool dlpScreenshotBlockLocked READ dlpScreenshotBlockLocked NOTIFY settingsChanged)

    Q_PROPERTY(bool dlpFileSandbox READ dlpFileSandbox NOTIFY settingsChanged)
    Q_PROPERTY(bool dlpFileSandboxLocked READ dlpFileSandboxLocked NOTIFY settingsChanged)

    Q_PROPERTY(bool dlpNetworkSandbox READ dlpNetworkSandbox NOTIFY settingsChanged)
    Q_PROPERTY(bool dlpNetworkSandboxLocked READ dlpNetworkSandboxLocked NOTIFY settingsChanged)

    // File transfer
    Q_PROPERTY(bool disableFileUpload READ disableFileUpload NOTIFY settingsChanged)
    Q_PROPERTY(bool disableFileUploadLocked READ disableFileUploadLocked NOTIFY settingsChanged)

    Q_PROPERTY(bool disableFileDownload READ disableFileDownload NOTIFY settingsChanged)
    Q_PROPERTY(bool disableFileDownloadLocked READ disableFileDownloadLocked NOTIFY settingsChanged)

    // UI
    Q_PROPERTY(QString theme READ theme NOTIFY settingsChanged)
    Q_PROPERTY(bool themeLocked READ themeLocked NOTIFY settingsChanged)

    Q_PROPERTY(bool notificationsEnabled READ notificationsEnabled NOTIFY settingsChanged)
    Q_PROPERTY(bool notificationsEnabledLocked READ notificationsEnabledLocked NOTIFY settingsChanged)

    // MDM meta
    Q_PROPERTY(bool mdmEnabled READ mdmEnabled NOTIFY settingsChanged)

    // -- Construction -----------------------------------------------------

    /// @param mdmPolicyPath  Override for the MDM JSON path (useful for tests).
    /// @param userSettingsPath  Override for the QSettings file path (useful for tests).
    /// @param parent  QObject parent.
    explicit SettingsManager(const QString& mdmPolicyPath = {},
                             const QString& userSettingsPath = {},
                             QObject* parent = nullptr);

    // -- Getters (resolved value) -----------------------------------------

    QString     deploymentUrl()         const;
    QStringList allowedDeployments()    const;
    bool        disableMultiDeployment() const;

    bool requireVpn()       const;
    bool autoConnectVpn()   const;

    bool dlpEnabled()        const;
    bool dlpClipboardBlock() const;
    bool dlpScreenshotBlock() const;
    bool dlpFileSandbox()    const;
    bool dlpNetworkSandbox() const;

    bool disableFileUpload()   const;
    bool disableFileDownload() const;

    QString theme()                const;
    bool    notificationsEnabled() const;

    bool mdmEnabled() const;

    // -- Locked getters ---------------------------------------------------

    bool deploymentUrlLocked()         const;
    bool allowedDeploymentsLocked()    const;
    bool disableMultiDeploymentLocked() const;
    bool requireVpnLocked()            const;
    bool autoConnectVpnLocked()        const;
    bool dlpEnabledLocked()            const;
    bool dlpClipboardBlockLocked()     const;
    bool dlpScreenshotBlockLocked()    const;
    bool dlpFileSandboxLocked()        const;
    bool dlpNetworkSandboxLocked()     const;
    bool disableFileUploadLocked()     const;
    bool disableFileDownloadLocked()   const;
    bool themeLocked()                 const;
    bool notificationsEnabledLocked()  const;

    // -- Invokables -------------------------------------------------------

    /// Set a user preference.  No-op if the key is MDM-locked.
    Q_INVOKABLE void setUserPreference(const QString& key, const QVariant& value);

    /// Returns true if the key is locked by MDM policy.
    Q_INVOKABLE bool isLocked(const QString& key) const;

    /// Returns the Source enum (as int) for QML consumption.
    Q_INVOKABLE int settingSource(const QString& key) const;

signals:
    void settingsChanged();

private:
    /// Resolve a setting through MDM → user → default layers.
    QVariant resolve(const QString& key, const QVariant& compiledDefault) const;

    /// The compiled-default value for a given key.
    static QVariant compiledDefault(const QString& key);

    std::unique_ptr<MdmConfigManager> m_mdm;
    std::unique_ptr<QSettings> m_userSettings;
};

#endif // SETTINGSMANAGER_H
