#include "data/SessionManager.h"

#include "api/CoderApiClient.h"
#include "api/dto/User.h"
#include "data/SecureStorage.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QSettings>
#include <QUuid>

// Token validation interval: 30 minutes.
static constexpr int kTokenValidateIntervalMs = 30 * 60 * 1000;

SessionManager::SessionManager(CoderApiClient &apiClient, SecureStorage &storage,
                               QObject *parent)
    : QObject(parent)
    , m_apiClient(apiClient)
    , m_storage(storage)
    , m_tokenValidator(new QTimer(this))
{
    loadDeployments();

    // Restore active deployment credentials into the API client.
    if (!m_activeDeployment.url.isEmpty()) {
        const QString token = m_storage.retrieveToken(m_activeDeployment.url);
        if (!token.isEmpty()) {
            m_apiClient.setBaseUrl(m_activeDeployment.url);
            m_apiClient.setSessionToken(token);
            startTokenValidation();
        }
    }
}

// ---------------------------------------------------------------------------
// Properties
// ---------------------------------------------------------------------------

bool SessionManager::isAuthenticated() const
{
    return m_apiClient.isAuthenticated();
}

QString SessionManager::currentUrl() const
{
    return m_activeDeployment.url;
}

QString SessionManager::currentUsername() const
{
    return m_activeDeployment.username;
}

QString SessionManager::sessionToken()
{
    if (m_activeDeployment.url.isEmpty())
        return {};
    return m_storage.retrieveToken(m_activeDeployment.url);
}

// ---------------------------------------------------------------------------
// Login / Logout
// ---------------------------------------------------------------------------

void SessionManager::login(const QString &url, const QString &token)
{
    // Configure the API client immediately so the request uses these creds.
    m_apiClient.setBaseUrl(url);
    m_apiClient.setSessionToken(token);

    QNetworkReply *reply = m_apiClient.getAuthenticatedUser();
    connect(reply, &QNetworkReply::finished, this, [this, reply, url, token]() {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            // Roll back API client state.
            m_apiClient.setSessionToken(QString());
            emit loginFailed(reply->errorString());
            return;
        }

        const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        const User user = User::fromJson(doc.object());

        // Store token securely.
        (void)m_storage.storeToken(url, token);

        // Build / update the deployment entry.
        Deployment dep;
        dep.url = url;
        dep.username = user.username;
        dep.avatarUrl = user.avatarUrl;
        dep.id = user.id;
        dep.name = url;  // use URL as display name
        dep.isActive = true;
        dep.addedAt = QDateTime::currentDateTimeUtc();

        // Deactivate any previously active deployment.
        for (auto &d : m_deployments)
            d.isActive = false;

        // Replace existing entry for the same URL, or append.
        bool found = false;
        for (int i = 0; i < m_deployments.size(); ++i) {
            if (m_deployments[i].url == url) {
                m_deployments[i] = dep;
                found = true;
                break;
            }
        }
        if (!found)
            m_deployments.append(dep);

        m_activeDeployment = dep;
        saveDeployments();
        startTokenValidation();

        emit authStateChanged();
        emit loginSucceeded(user.username);
    });
}

void SessionManager::logout()
{
    if (!m_activeDeployment.url.isEmpty())
        (void)m_storage.removeToken(m_activeDeployment.url);

    m_apiClient.setSessionToken(QString());
    m_apiClient.setBaseUrl(QString());

    // Mark the deployment inactive but keep it in the list.
    for (auto &d : m_deployments) {
        if (d.url == m_activeDeployment.url)
            d.isActive = false;
    }
    m_activeDeployment = Deployment{};
    saveDeployments();

    m_tokenValidator->stop();
    emit authStateChanged();
}

// ---------------------------------------------------------------------------
// Token validation
// ---------------------------------------------------------------------------

void SessionManager::validateToken()
{
    if (!isAuthenticated())
        return;

    QNetworkReply *reply = m_apiClient.getAuthenticatedUser();
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();

        const int status =
            reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (status == 401) {
            m_tokenValidator->stop();
            emit tokenExpired();
        }
    });
}

void SessionManager::startTokenValidation()
{
    m_tokenValidator->stop();
    connect(m_tokenValidator, &QTimer::timeout,
            this, &SessionManager::validateToken, Qt::UniqueConnection);
    m_tokenValidator->start(kTokenValidateIntervalMs);
}

// ---------------------------------------------------------------------------
// Multi-deployment
// ---------------------------------------------------------------------------

QVariantList SessionManager::deployments() const
{
    QVariantList list;
    for (const auto &d : m_deployments) {
        QVariantMap m;
        m[QStringLiteral("id")] = d.id;
        m[QStringLiteral("name")] = d.name;
        m[QStringLiteral("url")] = d.url;
        m[QStringLiteral("username")] = d.username;
        m[QStringLiteral("avatarUrl")] = d.avatarUrl;
        m[QStringLiteral("isActive")] = d.isActive;
        m[QStringLiteral("addedAt")] = d.addedAt.toString(Qt::ISODate);
        list.append(m);
    }
    return list;
}

void SessionManager::switchDeployment(const QString &url)
{
    // Find the deployment.
    Deployment *target = nullptr;
    for (auto &d : m_deployments) {
        if (d.url == url) {
            target = &d;
            break;
        }
    }
    if (!target)
        return;

    const QString token = m_storage.retrieveToken(url);
    if (token.isEmpty()) {
        emit loginFailed(QStringLiteral("No stored token for %1").arg(url));
        return;
    }

    // Deactivate all, activate the target.
    for (auto &d : m_deployments)
        d.isActive = false;
    target->isActive = true;
    m_activeDeployment = *target;

    m_apiClient.setBaseUrl(url);
    m_apiClient.setSessionToken(token);
    saveDeployments();
    startTokenValidation();

    emit authStateChanged();
}

void SessionManager::removeDeployment(const QString &url)
{
    (void)m_storage.removeToken(url);

    m_deployments.erase(
        std::remove_if(m_deployments.begin(), m_deployments.end(),
                       [&url](const Deployment &d) { return d.url == url; }),
        m_deployments.end());

    // If we just removed the active deployment, log out.
    if (m_activeDeployment.url == url) {
        m_activeDeployment = Deployment{};
        m_apiClient.setSessionToken(QString());
        m_apiClient.setBaseUrl(QString());
        m_tokenValidator->stop();
        emit authStateChanged();
    }

    saveDeployments();
}

// ---------------------------------------------------------------------------
// Persistence — deployments stored as JSON array in QSettings
// ---------------------------------------------------------------------------

void SessionManager::loadDeployments()
{
    QSettings settings;
    const QString raw = settings.value(QStringLiteral("deployments")).toString();
    if (raw.isEmpty())
        return;

    const QJsonDocument doc = QJsonDocument::fromJson(raw.toUtf8());
    if (!doc.isArray())
        return;

    const QJsonArray arr = doc.array();
    for (const auto &val : arr) {
        const QJsonObject obj = val.toObject();
        Deployment d;
        d.id = obj.value(QLatin1String("id")).toString();
        d.name = obj.value(QLatin1String("name")).toString();
        d.url = obj.value(QLatin1String("url")).toString();
        d.username = obj.value(QLatin1String("username")).toString();
        d.avatarUrl = obj.value(QLatin1String("avatarUrl")).toString();
        d.isActive = obj.value(QLatin1String("isActive")).toBool();
        d.addedAt = QDateTime::fromString(
            obj.value(QLatin1String("addedAt")).toString(), Qt::ISODate);
        m_deployments.append(d);

        if (d.isActive)
            m_activeDeployment = d;
    }
}

void SessionManager::saveDeployments()
{
    QJsonArray arr;
    for (const auto &d : m_deployments) {
        QJsonObject obj;
        obj[QLatin1String("id")] = d.id;
        obj[QLatin1String("name")] = d.name;
        obj[QLatin1String("url")] = d.url;
        obj[QLatin1String("username")] = d.username;
        obj[QLatin1String("avatarUrl")] = d.avatarUrl;
        obj[QLatin1String("isActive")] = d.isActive;
        obj[QLatin1String("addedAt")] = d.addedAt.toString(Qt::ISODate);
        arr.append(obj);
    }

    QSettings settings;
    settings.setValue(QStringLiteral("deployments"),
                     QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact)));
}
