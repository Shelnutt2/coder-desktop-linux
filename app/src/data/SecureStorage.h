#ifndef SECURESTORAGE_H
#define SECURESTORAGE_H

#include <QObject>
#include <QString>
#include <QStringList>

/// Credential storage with a two-backend cascade:
///   1. libsecret (D-Bus Secret Service API) — if available at compile-time
///      and the secret service daemon is reachable at run-time.
///   2. Fallback file — base64-encoded JSON in
///      ~/.config/coder-desktop/credentials.json.
///      NOT truly secure; a warning is logged on first use.
///
/// The fallback exists so headless / no-keyring environments still work.
class SecureStorage : public QObject {
    Q_OBJECT

public:
    explicit SecureStorage(QObject* parent = nullptr);

    /// Store a session token for a Coder deployment URL.
    [[nodiscard]] bool storeToken(const QString& deploymentUrl, const QString& token);

    /// Retrieve a previously stored token (empty string on miss).
    [[nodiscard]] QString retrieveToken(const QString& deploymentUrl);

    /// Remove the token for a deployment URL.
    [[nodiscard]] bool removeToken(const QString& deploymentUrl);

    /// List all deployment URLs that have stored tokens.
    [[nodiscard]] QStringList storedDeploymentUrls();

    /// Returns true when the secure backend (libsecret) is usable.
    [[nodiscard]] bool isSecureBackendAvailable() const;

private:
    enum class Backend { LibSecret, EncryptedFile, None };
    Backend m_backend = Backend::None;
    QString m_fallbackPath;

    Backend detectBackend();

#ifdef HAS_LIBSECRET
    bool libsecretStore(const QString& key, const QString& value);
    QString libsecretRetrieve(const QString& key);
    bool libsecretRemove(const QString& key);
    QStringList libsecretListKeys();
#endif

    // Fallback: XDG config dir file (base64 encoded — not secure)
    bool fileStore(const QString& key, const QString& value);
    QString fileRetrieve(const QString& key);
    bool fileRemove(const QString& key);
    QStringList fileListKeys();

    /// Load the fallback JSON file into a QJsonObject.
    QJsonObject loadFallbackFile() const;
    /// Save a QJsonObject to the fallback file.
    bool saveFallbackFile(const QJsonObject& obj) const;
};

#endif  // SECURESTORAGE_H
