#include "settings/MdmConfigManager.h"

#include <QDebug>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

MdmConfigManager::MdmConfigManager(const QString& policyPath) : m_policyPath(policyPath) {
    parse();
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

QVariant MdmConfigManager::value(const QString& key) const {
    if (!m_enabled) return {};

    const QJsonValue entry = m_settings.value(key);
    if (!entry.isObject()) return {};

    return entry.toObject().value(QStringLiteral("value")).toVariant();
}

bool MdmConfigManager::isLocked(const QString& key) const {
    if (!m_enabled) return false;

    const QJsonValue entry = m_settings.value(key);
    if (!entry.isObject()) return false;

    return entry.toObject().value(QStringLiteral("locked")).toBool(false);
}

void MdmConfigManager::reload() {
    m_enabled = false;
    m_settings = {};
    parse();
}

// ---------------------------------------------------------------------------
// Internal
// ---------------------------------------------------------------------------

void MdmConfigManager::parse() {
    QFile file(m_policyPath);
    if (!file.exists()) {
        qDebug() << "MdmConfigManager: policy file does not exist:" << m_policyPath;
        return;
    }
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "MdmConfigManager: failed to open policy file:" << m_policyPath;
        return;
    }

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    file.close();

    if (parseError.error != QJsonParseError::NoError) {
        qWarning() << "MdmConfigManager: JSON parse error:" << parseError.errorString();
        return;
    }
    if (!doc.isObject()) {
        qWarning() << "MdmConfigManager: root is not a JSON object";
        return;
    }

    const QJsonObject root = doc.object();
    const int version = root.value(QStringLiteral("version")).toInt(0);
    if (version != 1) {
        qWarning() << "MdmConfigManager: unsupported policy version" << version;
        return;
    }

    const QJsonValue settingsVal = root.value(QStringLiteral("settings"));
    if (!settingsVal.isObject()) {
        qWarning() << "MdmConfigManager: 'settings' is not an object";
        return;
    }

    m_settings = settingsVal.toObject();
    m_enabled = true;
    qDebug() << "MdmConfigManager: loaded policy with" << m_settings.count() << "setting(s)";
}
