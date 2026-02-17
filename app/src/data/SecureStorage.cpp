// Include libsecret BEFORE any Qt headers to avoid the glib "signals"
// struct member colliding with Qt's `#define signals public`.
#ifdef HAS_LIBSECRET
#include <libsecret/secret.h>
#endif

#include "data/SecureStorage.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

// ---------------------------------------------------------------------------
// libsecret schema (compile-time only)
// ---------------------------------------------------------------------------

#ifdef HAS_LIBSECRET
static const SecretSchema* coderSchema() {
    static const SecretSchema schema = {
        "com.coder.desktop.credentials",
        SECRET_SCHEMA_NONE,
        {
            {"deployment_url", SECRET_SCHEMA_ATTRIBUTE_STRING},
            {nullptr, SecretSchemaAttributeType(0)},
        },
    };
    return &schema;
}
#endif

// ---------------------------------------------------------------------------
// Construction & detection
// ---------------------------------------------------------------------------

SecureStorage::SecureStorage(QObject* parent) : QObject(parent), m_backend(detectBackend()) {
    // Fallback file lives under the XDG config directory.
    const QString configDir =
        QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) +
        QStringLiteral("/coder-desktop");
    QDir().mkpath(configDir);
    m_fallbackPath = configDir + QStringLiteral("/credentials.json");

    if (m_backend == Backend::EncryptedFile) {
        qWarning() << "SecureStorage: using INSECURE file-based fallback at" << m_fallbackPath
                   << "— credentials are only base64-encoded, not encrypted.";
    }
    qDebug() << "SecureStorage: backend =" << static_cast<int>(m_backend);
}

SecureStorage::Backend SecureStorage::detectBackend() {
#ifdef HAS_LIBSECRET
    // Try a quick round-trip to see if the secret service is reachable.
    GError* err = nullptr;
    gchar* test = secret_password_lookup_sync(coderSchema(), nullptr, &err, "deployment_url",
                                              "__coder_desktop_probe__", nullptr);
    if (err) {
        qDebug() << "SecureStorage: libsecret probe failed:" << err->message;
        g_error_free(err);
    } else {
        secret_password_free(test);
        return Backend::LibSecret;
    }
#endif
    return Backend::EncryptedFile;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

bool SecureStorage::isSecureBackendAvailable() const {
    return m_backend == Backend::LibSecret;
}

bool SecureStorage::storeToken(const QString& deploymentUrl, const QString& token) {
#ifdef HAS_LIBSECRET
    if (m_backend == Backend::LibSecret) {
        if (!libsecretStore(deploymentUrl, token)) return false;
        // Mirror the URL (not the token) to the fallback file so that
        // storedDeploymentUrls() works — libsecret has no schema-scoped
        // enumeration in its simple API.
        QJsonObject root = loadFallbackFile();
        root.insert(deploymentUrl, QStringLiteral("libsecret"));
        saveFallbackFile(root);
        return true;
    }
#endif
    return fileStore(deploymentUrl, token);
}

QString SecureStorage::retrieveToken(const QString& deploymentUrl) {
#ifdef HAS_LIBSECRET
    if (m_backend == Backend::LibSecret) return libsecretRetrieve(deploymentUrl);
#endif
    return fileRetrieve(deploymentUrl);
}

bool SecureStorage::removeToken(const QString& deploymentUrl) {
#ifdef HAS_LIBSECRET
    if (m_backend == Backend::LibSecret) {
        bool removed = libsecretRemove(deploymentUrl);
        // Also remove the mirrored URL entry from the fallback file.
        QJsonObject root = loadFallbackFile();
        root.remove(deploymentUrl);
        saveFallbackFile(root);
        return removed;
    }
#endif
    return fileRemove(deploymentUrl);
}

QStringList SecureStorage::storedDeploymentUrls() {
#ifdef HAS_LIBSECRET
    if (m_backend == Backend::LibSecret) return libsecretListKeys();
#endif
    return fileListKeys();
}

// ---------------------------------------------------------------------------
// libsecret backend
// ---------------------------------------------------------------------------

#ifdef HAS_LIBSECRET

bool SecureStorage::libsecretStore(const QString& key, const QString& value) {
    GError* err = nullptr;
    secret_password_store_sync(coderSchema(), SECRET_COLLECTION_DEFAULT,
                               QStringLiteral("Coder Desktop — %1").arg(key).toUtf8().constData(),
                               value.toUtf8().constData(), nullptr, &err, "deployment_url",
                               key.toUtf8().constData(), nullptr);
    if (err) {
        qWarning() << "SecureStorage: libsecret store failed:" << err->message;
        g_error_free(err);
        return false;
    }
    return true;
}

QString SecureStorage::libsecretRetrieve(const QString& key) {
    GError* err = nullptr;
    gchar* password = secret_password_lookup_sync(coderSchema(), nullptr, &err, "deployment_url",
                                                  key.toUtf8().constData(), nullptr);
    if (err) {
        qWarning() << "SecureStorage: libsecret lookup failed:" << err->message;
        g_error_free(err);
        return {};
    }
    if (!password) return {};
    QString result = QString::fromUtf8(password);
    secret_password_free(password);
    return result;
}

bool SecureStorage::libsecretRemove(const QString& key) {
    GError* err = nullptr;
    gboolean removed = secret_password_clear_sync(coderSchema(), nullptr, &err, "deployment_url",
                                                  key.toUtf8().constData(), nullptr);
    if (err) {
        qWarning() << "SecureStorage: libsecret remove failed:" << err->message;
        g_error_free(err);
        return false;
    }
    return removed;
}

QStringList SecureStorage::libsecretListKeys() {
    // libsecret doesn't have a convenient "list by schema" in the simple API.
    // We also keep a mirror in the fallback file for enumeration.
    // This is a pragmatic compromise: the actual secrets live in the keyring,
    // and we only store the *URL list* (no tokens) in the file.
    return fileListKeys();
}

#endif  // HAS_LIBSECRET

// ---------------------------------------------------------------------------
// File-based fallback
// ---------------------------------------------------------------------------

QJsonObject SecureStorage::loadFallbackFile() const {
    QFile f(m_fallbackPath);
    if (!f.open(QIODevice::ReadOnly)) return {};
    return QJsonDocument::fromJson(f.readAll()).object();
}

bool SecureStorage::saveFallbackFile(const QJsonObject& obj) const {
    QFile f(m_fallbackPath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qWarning() << "SecureStorage: cannot write" << m_fallbackPath;
        return false;
    }
    f.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
    // Best-effort permission restriction.
    f.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    return true;
}

bool SecureStorage::fileStore(const QString& key, const QString& value) {
    QJsonObject root = loadFallbackFile();
    // Values are base64-encoded so casual inspection doesn't reveal tokens.
    root.insert(key, QString::fromLatin1(value.toUtf8().toBase64()));
    return saveFallbackFile(root);
}

QString SecureStorage::fileRetrieve(const QString& key) {
    const QJsonObject root = loadFallbackFile();
    const QString encoded = root.value(key).toString();
    if (encoded.isEmpty()) return {};
    return QString::fromUtf8(QByteArray::fromBase64(encoded.toLatin1()));
}

bool SecureStorage::fileRemove(const QString& key) {
    QJsonObject root = loadFallbackFile();
    if (!root.contains(key)) return false;
    root.remove(key);
    return saveFallbackFile(root);
}

QStringList SecureStorage::fileListKeys() {
    return loadFallbackFile().keys();
}
