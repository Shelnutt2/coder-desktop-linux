#ifndef SESSIONMANAGER_H
#define SESSIONMANAGER_H

#include <QList>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QVariantList>

#include "data/Deployment.h"

class CoderApiClient;
class SecureStorage;

/// Coordinates authentication state, credential storage, and deployment
/// switching.  Exposes key auth properties to QML via Q_PROPERTY.
///
/// Login flow:
///   1. QML calls login(url, token)
///   2. SessionManager configures CoderApiClient and calls GET /api/v2/users/me
///   3. On success → store token in SecureStorage, persist deployment, emit loginSucceeded
///   4. On failure → emit loginFailed
///
/// Token validation runs every 30 minutes.  A 401 response emits tokenExpired().
class SessionManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool authenticated READ isAuthenticated NOTIFY authStateChanged)
    Q_PROPERTY(QString currentUrl READ currentUrl NOTIFY authStateChanged)
    Q_PROPERTY(QString currentUsername READ currentUsername NOTIFY authStateChanged)

public:
    explicit SessionManager(CoderApiClient &apiClient, SecureStorage &storage,
                            QObject *parent = nullptr);

    [[nodiscard]] bool isAuthenticated() const;
    [[nodiscard]] QString currentUrl() const;
    [[nodiscard]] QString currentUsername() const;

    /// Returns the current session token (retrieved from SecureStorage).
    /// Used by VPN bridge to authenticate with the Coder deployment.
    [[nodiscard]] Q_INVOKABLE QString sessionToken();

    Q_INVOKABLE void login(const QString &url, const QString &token);
    Q_INVOKABLE void logout();
    Q_INVOKABLE void validateToken();

    // Multi-deployment
    [[nodiscard]] Q_INVOKABLE QVariantList deployments() const;
    Q_INVOKABLE void switchDeployment(const QString &url);
    Q_INVOKABLE void removeDeployment(const QString &url);

signals:
    void authStateChanged();
    void tokenExpired();
    void loginSucceeded(const QString &username);
    void loginFailed(const QString &error);

private:
    CoderApiClient &m_apiClient;   // non-owning reference
    SecureStorage &m_storage;      // non-owning reference
    Deployment m_activeDeployment;
    QList<Deployment> m_deployments;
    QTimer *m_tokenValidator;      // Qt parent-owned (this)

    void loadDeployments();
    void saveDeployments();
    void startTokenValidation();
};

#endif // SESSIONMANAGER_H
