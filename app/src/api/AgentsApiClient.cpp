#include "api/AgentsApiClient.h"

#include <QJsonDocument>
#include <QLoggingCategory>
#include <QUrlQuery>

Q_LOGGING_CATEGORY(lcAgentsApi, "coder.agents.api")

namespace {
constexpr auto kChatsBase = "/api/experimental/chats";
}

// ---------------------------------------------------------------------------
// Construction / configuration
// ---------------------------------------------------------------------------

AgentsApiClient::AgentsApiClient(QObject* parent)
    : QObject(parent), m_nam(new QNetworkAccessManager(this)) {}

void AgentsApiClient::setBaseUrl(const QString& url) {
    QString normalised = url;
    while (normalised.endsWith(QLatin1Char('/'))) normalised.chop(1);
    if (m_baseUrl != normalised) {
        m_baseUrl = normalised;
        emit baseUrlChanged();
    }
}

void AgentsApiClient::setSessionToken(const QString& token) {
    m_sessionToken = token;
}

// ---------------------------------------------------------------------------
// Chats
// ---------------------------------------------------------------------------

void AgentsApiClient::listChats(const QString& query, const QVariantMap& labels, int limit,
                                int offset) {
    QUrlQuery q;
    if (!query.isEmpty()) q.addQueryItem(QStringLiteral("q"), query);
    for (auto it = labels.constBegin(); it != labels.constEnd(); ++it) {
        q.addQueryItem(QStringLiteral("label"),
                       it.key() + QLatin1Char(':') + it.value().toString());
    }
    if (limit > 0) q.addQueryItem(QStringLiteral("limit"), QString::number(limit));
    if (offset > 0) q.addQueryItem(QStringLiteral("offset"), QString::number(offset));

    QString path = QLatin1String(kChatsBase);
    if (!q.isEmpty()) path += QLatin1Char('?') + q.toString(QUrl::FullyEncoded);

    QNetworkReply* reply = get(path);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) return;
        const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        emit chatsJsonReceived(doc.array());
        emit chatsReceived(Chat::listFromJson(doc.array()));
    });
}

void AgentsApiClient::createChat(const QJsonObject& request) {
    QNetworkReply* reply = post(QLatin1String(kChatsBase), request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) return;
        const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        emit chatCreated(Chat::fromJson(doc.object()));
    });
}

void AgentsApiClient::getChat(const QString& chatId) {
    QNetworkReply* reply = get(QStringLiteral("%1/%2").arg(QLatin1String(kChatsBase), chatId));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) return;
        const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        emit chatReceived(Chat::fromJson(doc.object()));
    });
}

void AgentsApiClient::updateChat(const QString& chatId, const QJsonObject& patchBody) {
    QNetworkReply* reply =
        patch(QStringLiteral("%1/%2").arg(QLatin1String(kChatsBase), chatId), patchBody);
    connect(reply, &QNetworkReply::finished, this, [this, reply, chatId]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) return;
        emit chatUpdated(chatId);
    });
}

// ---------------------------------------------------------------------------
// Messages
// ---------------------------------------------------------------------------

void AgentsApiClient::listMessages(const QString& chatId, qint64 beforeId, qint64 afterId,
                                   int limit) {
    QUrlQuery q;
    if (beforeId > 0) q.addQueryItem(QStringLiteral("before_id"), QString::number(beforeId));
    if (afterId > 0) q.addQueryItem(QStringLiteral("after_id"), QString::number(afterId));
    if (limit > 0) q.addQueryItem(QStringLiteral("limit"), QString::number(limit));

    QString path = QStringLiteral("%1/%2/messages").arg(QLatin1String(kChatsBase), chatId);
    if (!q.isEmpty()) path += QLatin1Char('?') + q.toString(QUrl::FullyEncoded);

    QNetworkReply* reply = get(path);
    connect(reply, &QNetworkReply::finished, this, [this, reply, chatId, beforeId, afterId]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) return;
        const QJsonObject obj = QJsonDocument::fromJson(reply->readAll()).object();
        emit messagesReceived(
            chatId, ChatMessage::listFromJson(obj.value(QLatin1String("messages")).toArray()),
            ChatQueuedMessage::listFromJson(obj.value(QLatin1String("queued_messages")).toArray()),
            obj.value(QLatin1String("has_more")).toBool(), beforeId, afterId);
    });
}

void AgentsApiClient::sendMessage(const QString& chatId, const QJsonArray& content,
                                  const QString& busyBehavior, const QJsonObject& extra) {
    QJsonObject body = extra;
    body.insert(QLatin1String("content"), content);
    if (!busyBehavior.isEmpty()) body.insert(QLatin1String("busy_behavior"), busyBehavior);

    const QString path = QStringLiteral("%1/%2/messages").arg(QLatin1String(kChatsBase), chatId);
    QNetworkReply* reply =
        m_nam->post(buildRequest(path), QJsonDocument(body).toJson(QJsonDocument::Compact));
    // No connectErrorHandler: HTTP 409 is a structured usage-limit response,
    // not a generic failure, so error routing is handled inline.
    connect(reply, &QNetworkReply::finished, this, [this, reply, chatId, path]() {
        reply->deleteLater();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray payload = reply->readAll();
        if (status == 409) {
            const QJsonObject obj = QJsonDocument::fromJson(payload).object();
            emit usageLimitExceeded(chatId, ChatUsageLimitExceeded::fromJson(obj));
            return;
        }
        if (reply->error() != QNetworkReply::NoError) {
            emit requestFailed(path, status, reply->errorString(), payload);
            return;
        }
        const QJsonObject obj = QJsonDocument::fromJson(payload).object();
        emit messageSent(chatId, obj.value(QLatin1String("queued")).toBool(), obj);
    });
}

void AgentsApiClient::editMessage(const QString& chatId, qint64 messageId,
                                  const QJsonArray& content) {
    QJsonObject body;
    body.insert(QLatin1String("content"), content);
    const QString path = QStringLiteral("%1/%2/messages/%3")
                             .arg(QLatin1String(kChatsBase), chatId, QString::number(messageId));
    QNetworkReply* reply = patch(path, body);
    connect(reply, &QNetworkReply::finished, this, [this, reply, chatId]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) return;
        emit messageEdited(chatId, QJsonDocument::fromJson(reply->readAll()).object());
    });
}

// ---------------------------------------------------------------------------
// Run control
// ---------------------------------------------------------------------------

void AgentsApiClient::interrupt(const QString& chatId) {
    QNetworkReply* reply =
        post(QStringLiteral("%1/%2/interrupt").arg(QLatin1String(kChatsBase), chatId));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) return;
        emit chatReceived(Chat::fromJson(QJsonDocument::fromJson(reply->readAll()).object()));
    });
}

void AgentsApiClient::deleteQueued(const QString& chatId, qint64 queuedId) {
    QNetworkReply* reply =
        del(QStringLiteral("%1/%2/queue/%3")
                .arg(QLatin1String(kChatsBase), chatId, QString::number(queuedId)));
    connect(reply, &QNetworkReply::finished, this, [this, reply, chatId, queuedId]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) return;
        emit queuedDeleted(chatId, queuedId);
    });
}

void AgentsApiClient::promoteQueued(const QString& chatId, qint64 queuedId) {
    QNetworkReply* reply =
        post(QStringLiteral("%1/%2/queue/%3/promote")
                 .arg(QLatin1String(kChatsBase), chatId, QString::number(queuedId)));
    connect(reply, &QNetworkReply::finished, this, [this, reply, chatId, queuedId]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) return;
        emit queuedPromoted(chatId, queuedId);
    });
}

void AgentsApiClient::sendToolResults(const QString& chatId, const QJsonArray& results) {
    QJsonObject body;
    body.insert(QLatin1String("results"), results);
    QNetworkReply* reply =
        post(QStringLiteral("%1/%2/tool-results").arg(QLatin1String(kChatsBase), chatId), body);
    connect(reply, &QNetworkReply::finished, this, [this, reply, chatId]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) return;
        emit toolResultsSent(chatId);
    });
}

// ---------------------------------------------------------------------------
// Titles / diff / prompts
// ---------------------------------------------------------------------------

void AgentsApiClient::regenerateTitle(const QString& chatId) {
    QNetworkReply* reply =
        post(QStringLiteral("%1/%2/title/regenerate").arg(QLatin1String(kChatsBase), chatId));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) return;
        emit chatReceived(Chat::fromJson(QJsonDocument::fromJson(reply->readAll()).object()));
    });
}

void AgentsApiClient::proposeTitle(const QString& chatId) {
    QNetworkReply* reply =
        post(QStringLiteral("%1/%2/title/propose").arg(QLatin1String(kChatsBase), chatId));
    connect(reply, &QNetworkReply::finished, this, [this, reply, chatId]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) return;
        const QJsonObject obj = QJsonDocument::fromJson(reply->readAll()).object();
        emit titleProposed(chatId, obj.value(QLatin1String("title")).toString());
    });
}

void AgentsApiClient::getDiff(const QString& chatId) {
    QNetworkReply* reply = get(QStringLiteral("%1/%2/diff").arg(QLatin1String(kChatsBase), chatId));
    connect(reply, &QNetworkReply::finished, this, [this, reply, chatId]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) return;
        emit diffReceived(chatId, QJsonDocument::fromJson(reply->readAll()).object());
    });
}

void AgentsApiClient::getPrompts(const QString& chatId, int limit) {
    QString path = QStringLiteral("%1/%2/prompts").arg(QLatin1String(kChatsBase), chatId);
    if (limit > 0) path += QStringLiteral("?limit=%1").arg(limit);
    QNetworkReply* reply = get(path);
    connect(reply, &QNetworkReply::finished, this, [this, reply, chatId]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) return;
        const QJsonObject obj = QJsonDocument::fromJson(reply->readAll()).object();
        QStringList prompts;
        const QJsonArray arr = obj.value(QLatin1String("prompts")).toArray();
        prompts.reserve(arr.size());
        for (const QJsonValue& v : arr)
            prompts.append(v.toObject().value(QLatin1String("text")).toString());
        emit promptsReceived(chatId, prompts);
    });
}

// ---------------------------------------------------------------------------
// Catalog / config
// ---------------------------------------------------------------------------

void AgentsApiClient::listModels() {
    QNetworkReply* reply = get(QStringLiteral("%1/models").arg(QLatin1String(kChatsBase)));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) return;
        emit modelsReceived(
            ChatModelsResponse::fromJson(QJsonDocument::fromJson(reply->readAll()).object()));
    });
}

void AgentsApiClient::listModelConfigs() {
    QNetworkReply* reply = get(QStringLiteral("%1/model-configs").arg(QLatin1String(kChatsBase)));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) return;
        emit modelConfigsReceived(
            ChatModelConfig::listFromJson(QJsonDocument::fromJson(reply->readAll()).array()));
    });
}

void AgentsApiClient::listMcpServers() {
    QNetworkReply* reply = get(QStringLiteral("/api/experimental/mcp/servers"));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) return;
        emit mcpServersReceived(
            McpServerConfig::listFromJson(QJsonDocument::fromJson(reply->readAll()).array()));
    });
}

void AgentsApiClient::usageLimitStatus() {
    QNetworkReply* reply =
        get(QStringLiteral("%1/usage-limits/status").arg(QLatin1String(kChatsBase)));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) return;
        emit usageLimitStatusReceived(
            ChatUsageLimitStatus::fromJson(QJsonDocument::fromJson(reply->readAll()).object()));
    });
}

// ---------------------------------------------------------------------------
// Files
// ---------------------------------------------------------------------------

void AgentsApiClient::uploadFile(const QString& organizationId, const QString& filename,
                                 const QString& contentType, const QByteArray& data) {
    // Raw-body upload: coderd/exp_chats.go postChatFile reads r.Body directly
    // and takes the filename from Content-Disposition, so no multipart
    // encoding is used.
    const QString path =
        QStringLiteral("%1/files?organization=%2").arg(QLatin1String(kChatsBase), organizationId);
    QNetworkRequest req = buildRequest(path);
    req.setHeader(QNetworkRequest::ContentTypeHeader,
                  contentType.isEmpty() ? QStringLiteral("application/octet-stream") : contentType);
    if (!filename.isEmpty()) {
        req.setRawHeader("Content-Disposition",
                         QStringLiteral("attachment; filename=\"%1\"").arg(filename).toUtf8());
    }
    QNetworkReply* reply = m_nam->post(req, data);
    connectErrorHandler(reply, path);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) return;
        const QJsonObject obj = QJsonDocument::fromJson(reply->readAll()).object();
        emit fileUploaded(obj.value(QLatin1String("id")).toString());
    });
}

void AgentsApiClient::downloadFile(const QString& fileId) {
    QNetworkReply* reply =
        get(QStringLiteral("%1/files/%2").arg(QLatin1String(kChatsBase), fileId));
    connect(reply, &QNetworkReply::finished, this, [this, reply, fileId]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) return;
        const QString contentType = reply->header(QNetworkRequest::ContentTypeHeader).toString();
        emit fileDownloaded(fileId, reply->readAll(), contentType);
    });
}

// ---------------------------------------------------------------------------
// Availability probing
// ---------------------------------------------------------------------------

void AgentsApiClient::checkCreateChatPermission() {
    QJsonObject object;
    object.insert(QLatin1String("resource_type"), QStringLiteral("chat"));
    QJsonObject check;
    check.insert(QLatin1String("object"), object);
    check.insert(QLatin1String("action"), QStringLiteral("create"));
    QJsonObject checks;
    checks.insert(QLatin1String("createChat"), check);
    QJsonObject body;
    body.insert(QLatin1String("checks"), checks);

    QNetworkReply* reply = post(QStringLiteral("/api/v2/authcheck"), body);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) return;
        const QJsonObject obj = QJsonDocument::fromJson(reply->readAll()).object();
        emit createChatPermissionReceived(obj.value(QLatin1String("createChat")).toBool());
    });
}

void AgentsApiClient::listOrganizations() {
    QNetworkReply* reply = get(QStringLiteral("/api/v2/organizations"));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) return;
        const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        emit organizationsReceived(doc.array());
    });
}

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

QNetworkRequest AgentsApiClient::buildRequest(const QString& path) const {
    QNetworkRequest req(QUrl(m_baseUrl + path));
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    if (!m_sessionToken.isEmpty()) req.setRawHeader("Coder-Session-Token", m_sessionToken.toUtf8());
    return req;
}

QNetworkReply* AgentsApiClient::get(const QString& path) {
    QNetworkReply* reply = m_nam->get(buildRequest(path));
    connectErrorHandler(reply, path);
    return reply;
}

QNetworkReply* AgentsApiClient::post(const QString& path, const QJsonObject& body) {
    QNetworkReply* reply =
        m_nam->post(buildRequest(path), QJsonDocument(body).toJson(QJsonDocument::Compact));
    connectErrorHandler(reply, path);
    return reply;
}

QNetworkReply* AgentsApiClient::post(const QString& path) {
    QNetworkReply* reply = m_nam->post(buildRequest(path), QByteArray());
    connectErrorHandler(reply, path);
    return reply;
}

QNetworkReply* AgentsApiClient::patch(const QString& path, const QJsonObject& body) {
    QNetworkReply* reply = m_nam->sendCustomRequest(
        buildRequest(path), "PATCH", QJsonDocument(body).toJson(QJsonDocument::Compact));
    connectErrorHandler(reply, path);
    return reply;
}

QNetworkReply* AgentsApiClient::del(const QString& path) {
    QNetworkReply* reply = m_nam->deleteResource(buildRequest(path));
    connectErrorHandler(reply, path);
    return reply;
}

void AgentsApiClient::connectErrorHandler(QNetworkReply* reply, const QString& endpoint) {
    connect(reply, &QNetworkReply::finished, this, [this, reply, endpoint]() {
        if (reply->error() == QNetworkReply::NoError) return;
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        // Peek the body without consuming it so success handlers connected
        // after this one still see the payload on non-error paths.
        const QByteArray body = reply->peek(reply->bytesAvailable());
        qCWarning(lcAgentsApi) << "request failed:" << endpoint << "status:" << status
                               << reply->errorString();
        emit requestFailed(endpoint, status, reply->errorString(), body);
    });
}
