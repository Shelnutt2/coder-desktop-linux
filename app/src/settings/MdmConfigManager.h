#ifndef MDMCONFIGMANAGER_H
#define MDMCONFIGMANAGER_H

#include <QJsonObject>
#include <QString>
#include <QVariant>

/// Reads MDM (Mobile Device Management) policy from a JSON file.
///
/// Expected JSON format:
/// @code
/// {
///   "version": 1,
///   "settings": {
///     "someKey": { "value": <any>, "locked": true }
///   }
/// }
/// @endcode
///
/// The default policy path is `/etc/coder-desktop/policy.json`.
/// The constructor accepts an optional override for testing.
class MdmConfigManager {
public:
    explicit MdmConfigManager(const QString& policyPath = QStringLiteral("/etc/coder-desktop/policy.json"));

    /// Returns true if the policy file exists and was successfully parsed.
    bool isEnabled() const { return m_enabled; }

    /// Returns the MDM value for @p key, or an invalid QVariant if absent.
    QVariant value(const QString& key) const;

    /// Returns true if the MDM policy locks @p key (prevents user override).
    bool isLocked(const QString& key) const;

    /// Re-read the policy file from disk.
    void reload();

private:
    QString m_policyPath;
    bool m_enabled = false;
    QJsonObject m_settings; ///< The "settings" object from policy JSON.

    void parse();
};

#endif // MDMCONFIGMANAGER_H
