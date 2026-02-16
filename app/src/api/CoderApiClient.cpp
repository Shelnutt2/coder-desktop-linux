#include "api/CoderApiClient.h"

#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QUrlQuery>

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

CoderApiClient::CoderApiClient(QObject *parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
{
}

// ---------------------------------------------------------------------------
// Properties
// ---------------------------------------------------------------------------

void CoderApiClient::setBaseUrl(const QString &url)
{
    // Normalise: strip trailing slash.
    QString normalised = url;
    while (normalised.endsWith(QLatin1Char('/')))
        normalised.chop(1);

    if (m_baseUrl != normalised) {
        m_baseUrl = normalised;
        emit baseUrlChanged();
    }
}

void CoderApiClient::setSessionToken(const QString &token)
{
    const bool wasAuth = isAuthenticated();
    m_sessionToken = token;
    if (wasAuth != isAuthenticated())
        emit authStateChanged();
}

// ---------------------------------------------------------------------------
// Public API methods
// ---------------------------------------------------------------------------

QNetworkReply *CoderApiClient::getAuthenticatedUser()
{
    return get(QStringLiteral("/api/v2/users/me"));
}

QNetworkReply *CoderApiClient::listWorkspaces(const QString &query)
{
    QString path = QStringLiteral("/api/v2/workspaces");
    if (!query.isEmpty()) {
        QUrlQuery q;
        q.addQueryItem(QStringLiteral("q"), query);
        path += QLatin1Char('?') + q.toString(QUrl::FullyEncoded);
    }
    return get(path);
}

QNetworkReply *CoderApiClient::getWorkspace(const QString &id)
{
    return get(QStringLiteral("/api/v2/workspaces/%1").arg(id));
}

QNetworkReply *CoderApiClient::createWorkspaceBuild(
    const QString &workspaceId,
    const QString &transition,
    const QString &templateVersionId)
{
    QJsonObject body;
    body.insert(QLatin1String("transition"), transition);
    if (!templateVersionId.isEmpty())
        body.insert(QLatin1String("template_version_id"), templateVersionId);

    return post(
        QStringLiteral("/api/v2/workspaces/%1/builds").arg(workspaceId),
        body);
}

QNetworkReply *CoderApiClient::deleteWorkspace(const QString &id)
{
    return del(QStringLiteral("/api/v2/workspaces/%1").arg(id));
}

QNetworkReply *CoderApiClient::favoriteWorkspace(const QString &id)
{
    return put(QStringLiteral("/api/v2/workspaces/%1/favorite").arg(id));
}

QNetworkReply *CoderApiClient::unfavoriteWorkspace(const QString &id)
{
    return del(QStringLiteral("/api/v2/workspaces/%1/favorite").arg(id));
}

QNetworkReply *CoderApiClient::listTemplates()
{
    return get(QStringLiteral("/api/v2/templates"));
}

QNetworkReply *CoderApiClient::listTasks()
{
    return get(QStringLiteral("/api/v2/tasks"));
}

QNetworkReply *CoderApiClient::getBuildInfo()
{
    return get(QStringLiteral("/api/v2/buildinfo"));
}

// ---------------------------------------------------------------------------
// High-level fetch methods (QML-callable)
// ---------------------------------------------------------------------------

void CoderApiClient::fetchWorkspaces()
{
    QNetworkReply *reply = listWorkspaces();
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError)
            return;  // requestFailed already emitted by connectErrorHandler

        const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        // The Coder API returns { "workspaces": [...], "count": N }
        const QJsonArray arr = doc.object()
            .value(QLatin1String("workspaces")).toArray();
        emit workspacesReceived(arr);
    });
}

void CoderApiClient::fetchTasks()
{
    QNetworkReply *reply = listTasks();
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError)
            return;  // requestFailed already emitted by connectErrorHandler

        const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        // The Coder tasks API returns { "tasks": [...] } or a flat array.
        QJsonArray arr;
        if (doc.isArray()) {
            arr = doc.array();
        } else {
            arr = doc.object().value(QLatin1String("tasks")).toArray();
        }
        emit tasksReceived(arr);
    });
}

void CoderApiClient::fetchWorkspaceDetail(const QString &id)
{
    QNetworkReply *reply = getWorkspace(id);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError)
            return;  // requestFailed already emitted by connectErrorHandler

        const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        emit workspaceDetailReceived(doc.object());
    });
}

void CoderApiClient::startWorkspace(const QString &id)
{
    QNetworkReply *reply = createWorkspaceBuild(id, QStringLiteral("start"));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError)
            return;  // requestFailed already emitted by connectErrorHandler

        emit workspaceActionCompleted();
    });
}

void CoderApiClient::stopWorkspace(const QString &id)
{
    QNetworkReply *reply = createWorkspaceBuild(id, QStringLiteral("stop"));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError)
            return;  // requestFailed already emitted by connectErrorHandler

        emit workspaceActionCompleted();
    });
}

void CoderApiClient::updateWorkspace(const QString &id)
{
    // TODO: Pass template_active_version_id for a real update transition.
    // For now this is equivalent to startWorkspace().
    startWorkspace(id);
}

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

QNetworkRequest CoderApiClient::buildRequest(const QString &path) const
{
    QUrl url(m_baseUrl + path);
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader,
                  QStringLiteral("application/json"));

    // Coder uses a custom header — NOT the Authorization: Bearer scheme.
    if (!m_sessionToken.isEmpty())
        req.setRawHeader("Coder-Session-Token", m_sessionToken.toUtf8());

    return req;
}

QNetworkReply *CoderApiClient::get(const QString &path)
{
    QNetworkReply *reply = m_nam->get(buildRequest(path));
    connectErrorHandler(reply, path);
    return reply;
}

QNetworkReply *CoderApiClient::post(const QString &path,
                                    const QJsonObject &body)
{
    QNetworkReply *reply = m_nam->post(
        buildRequest(path),
        QJsonDocument(body).toJson(QJsonDocument::Compact));
    connectErrorHandler(reply, path);
    return reply;
}

QNetworkReply *CoderApiClient::put(const QString &path,
                                   const QJsonObject &body)
{
    QNetworkReply *reply = m_nam->put(
        buildRequest(path),
        QJsonDocument(body).toJson(QJsonDocument::Compact));
    connectErrorHandler(reply, path);
    return reply;
}

QNetworkReply *CoderApiClient::del(const QString &path)
{
    QNetworkReply *reply = m_nam->deleteResource(buildRequest(path));
    connectErrorHandler(reply, path);
    return reply;
}

void CoderApiClient::connectErrorHandler(QNetworkReply *reply,
                                         const QString &endpoint)
{
    connect(reply, &QNetworkReply::finished, this, [this, reply, endpoint]() {
        const int status =
            reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (reply->error() != QNetworkReply::NoError) {
            const QString msg = reply->errorString();
            qWarning() << "CoderApiClient: request failed:" << endpoint
                       << "status:" << status << msg;
            emit requestFailed(endpoint, status, msg);
        }
        // Callers are responsible for deleteLater() on the reply.
    });
}
