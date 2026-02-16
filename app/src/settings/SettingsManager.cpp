#include "settings/SettingsManager.h"

#include <QDebug>
#include <QDir>
#include <QStandardPaths>

// ---------------------------------------------------------------------------
// Compiled defaults table
// ---------------------------------------------------------------------------

static const struct {
    const char* key;
    QVariant value;
} kDefaults[] = {
    { "deploymentUrl",          QString()          },
    { "allowedDeployments",     QStringList()      },
    { "disableMultiDeployment", false              },
    { "requireVpn",             false              },
    { "autoConnectVpn",         false              },
    { "dlpEnabled",             false              },
    { "dlpClipboardBlock",      false              },
    { "dlpScreenshotBlock",     false              },
    { "dlpFileSandbox",         false              },
    { "dlpNetworkSandbox",      false              },
    { "disableFileUpload",      false              },
    { "disableFileDownload",    false              },
    { "theme",                  QStringLiteral("system") },
    { "notificationsEnabled",   true               },
};

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

SettingsManager::SettingsManager(const QString& mdmPolicyPath,
                                 const QString& userSettingsPath,
                                 QObject* parent)
    : QObject(parent)
{
    // MDM policy
    const QString mdmPath = mdmPolicyPath.isEmpty()
        ? QStringLiteral("/etc/coder-desktop/policy.json")
        : mdmPolicyPath;
    m_mdm = std::make_unique<MdmConfigManager>(mdmPath);

    // User preferences (INI-format QSettings backed by a JSON-named file).
    if (userSettingsPath.isEmpty()) {
        const QString configDir =
            QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)
            + QStringLiteral("/coder-desktop");
        QDir().mkpath(configDir);
        m_userSettings = std::make_unique<QSettings>(
            configDir + QStringLiteral("/settings.json"),
            QSettings::IniFormat);
    } else {
        m_userSettings = std::make_unique<QSettings>(
            userSettingsPath, QSettings::IniFormat);
    }
}

// ---------------------------------------------------------------------------
// Compiled defaults helper
// ---------------------------------------------------------------------------

QVariant SettingsManager::compiledDefault(const QString& key)
{
    for (const auto& d : kDefaults) {
        if (key == QLatin1String(d.key))
            return d.value;
    }
    return {};
}

// ---------------------------------------------------------------------------
// Resolution
// ---------------------------------------------------------------------------

QVariant SettingsManager::resolve(const QString& key,
                                  const QVariant& compiled) const
{
    // Layer 1: MDM
    if (m_mdm->isEnabled()) {
        const QVariant mdmVal = m_mdm->value(key);
        if (mdmVal.isValid())
            return mdmVal;
    }

    // Layer 2: user preferences
    if (m_userSettings->contains(key))
        return m_userSettings->value(key);

    // Layer 3: compiled default
    return compiled;
}

// ---------------------------------------------------------------------------
// Property getters (resolved)
// ---------------------------------------------------------------------------

QString SettingsManager::deploymentUrl() const
{
    return resolve(QStringLiteral("deploymentUrl"), QString()).toString();
}

QStringList SettingsManager::allowedDeployments() const
{
    return resolve(QStringLiteral("allowedDeployments"), QStringList()).toStringList();
}

bool SettingsManager::disableMultiDeployment() const
{
    return resolve(QStringLiteral("disableMultiDeployment"), false).toBool();
}

bool SettingsManager::requireVpn() const
{
    return resolve(QStringLiteral("requireVpn"), false).toBool();
}

bool SettingsManager::autoConnectVpn() const
{
    return resolve(QStringLiteral("autoConnectVpn"), false).toBool();
}

bool SettingsManager::dlpEnabled() const
{
    return resolve(QStringLiteral("dlpEnabled"), false).toBool();
}

bool SettingsManager::dlpClipboardBlock() const
{
    return resolve(QStringLiteral("dlpClipboardBlock"), false).toBool();
}

bool SettingsManager::dlpScreenshotBlock() const
{
    return resolve(QStringLiteral("dlpScreenshotBlock"), false).toBool();
}

bool SettingsManager::dlpFileSandbox() const
{
    return resolve(QStringLiteral("dlpFileSandbox"), false).toBool();
}

bool SettingsManager::dlpNetworkSandbox() const
{
    return resolve(QStringLiteral("dlpNetworkSandbox"), false).toBool();
}

bool SettingsManager::disableFileUpload() const
{
    return resolve(QStringLiteral("disableFileUpload"), false).toBool();
}

bool SettingsManager::disableFileDownload() const
{
    return resolve(QStringLiteral("disableFileDownload"), false).toBool();
}

QString SettingsManager::theme() const
{
    return resolve(QStringLiteral("theme"), QStringLiteral("system")).toString();
}

bool SettingsManager::notificationsEnabled() const
{
    return resolve(QStringLiteral("notificationsEnabled"), true).toBool();
}

bool SettingsManager::mdmEnabled() const
{
    return m_mdm->isEnabled();
}

// ---------------------------------------------------------------------------
// Locked getters
// ---------------------------------------------------------------------------

bool SettingsManager::deploymentUrlLocked()         const { return m_mdm->isLocked(QStringLiteral("deploymentUrl")); }
bool SettingsManager::allowedDeploymentsLocked()    const { return m_mdm->isLocked(QStringLiteral("allowedDeployments")); }
bool SettingsManager::disableMultiDeploymentLocked() const { return m_mdm->isLocked(QStringLiteral("disableMultiDeployment")); }
bool SettingsManager::requireVpnLocked()            const { return m_mdm->isLocked(QStringLiteral("requireVpn")); }
bool SettingsManager::autoConnectVpnLocked()        const { return m_mdm->isLocked(QStringLiteral("autoConnectVpn")); }
bool SettingsManager::dlpEnabledLocked()            const { return m_mdm->isLocked(QStringLiteral("dlpEnabled")); }
bool SettingsManager::dlpClipboardBlockLocked()     const { return m_mdm->isLocked(QStringLiteral("dlpClipboardBlock")); }
bool SettingsManager::dlpScreenshotBlockLocked()    const { return m_mdm->isLocked(QStringLiteral("dlpScreenshotBlock")); }
bool SettingsManager::dlpFileSandboxLocked()        const { return m_mdm->isLocked(QStringLiteral("dlpFileSandbox")); }
bool SettingsManager::dlpNetworkSandboxLocked()     const { return m_mdm->isLocked(QStringLiteral("dlpNetworkSandbox")); }
bool SettingsManager::disableFileUploadLocked()     const { return m_mdm->isLocked(QStringLiteral("disableFileUpload")); }
bool SettingsManager::disableFileDownloadLocked()   const { return m_mdm->isLocked(QStringLiteral("disableFileDownload")); }
bool SettingsManager::themeLocked()                 const { return m_mdm->isLocked(QStringLiteral("theme")); }
bool SettingsManager::notificationsEnabledLocked()  const { return m_mdm->isLocked(QStringLiteral("notificationsEnabled")); }

// ---------------------------------------------------------------------------
// Invokables
// ---------------------------------------------------------------------------

void SettingsManager::setUserPreference(const QString& key,
                                        const QVariant& value)
{
    if (m_mdm->isLocked(key)) {
        qDebug() << "SettingsManager: ignoring setUserPreference for MDM-locked key:" << key;
        return;
    }

    m_userSettings->setValue(key, value);
    m_userSettings->sync();
    emit settingsChanged();
}

bool SettingsManager::isLocked(const QString& key) const
{
    return m_mdm->isLocked(key);
}

int SettingsManager::settingSource(const QString& key) const
{
    // MDM present and has a value for this key?
    if (m_mdm->isEnabled() && m_mdm->value(key).isValid())
        return static_cast<int>(Source::Mdm);

    // User preference set?
    if (m_userSettings->contains(key))
        return static_cast<int>(Source::User);

    return static_cast<int>(Source::Default);
}
