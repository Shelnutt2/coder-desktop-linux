#include "agents/AgentsController.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QMimeDatabase>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>

#include "agents/DiffParser.h"
#include "agents/JsonSchemaParser.h"
#include "agents/PlanStepParser.h"

Q_LOGGING_CATEGORY(lcAgents, "coder.agents.controller")

namespace {

const QString kDraftSettingsGroup = QStringLiteral("agentDrafts");
const QString kPrefLastWorkspace = QStringLiteral("agents/lastWorkspaceId");
const QString kPrefLastModelConfig = QStringLiteral("agents/lastModelConfigId");
const QString kPrefSendShortcut = QStringLiteral("agents/sendShortcut");

/// Parses an arbitrary JSON value from text: objects and arrays directly,
/// primitives by wrapping in a throwaway array. Falls back to the raw text
/// as a JSON string when the text is not valid JSON.
QJsonValue parseJsonValue(const QString& text) {
    const QByteArray utf8 = text.trimmed().toUtf8();
    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(utf8, &err);
    if (err.error == QJsonParseError::NoError) {
        if (doc.isObject()) return doc.object();
        if (doc.isArray()) return doc.array();
    }
    const QJsonDocument wrapped =
        QJsonDocument::fromJson(QByteArray("[") + utf8 + QByteArray("]"), &err);
    if (err.error == QJsonParseError::NoError && wrapped.isArray() && wrapped.array().size() == 1) {
        return wrapped.array().first();
    }
    return QJsonValue(text);
}

}  // namespace

// ===========================================================================
// ChatController
// ===========================================================================

ChatController::ChatController(AgentsApiClient* api, const QString& chatId, QObject* parent)
    : QObject(parent), m_api(api), m_chatId(chatId) {
    m_session = new ChatSession(chatId, this);
    m_stream = new ChatStreamWebSocket(this);
    m_model = new ChatMessagesModel(m_session, this);

    loadPersistedDraft();

    m_stream->setBaseUrl(m_api->baseUrl());
    m_stream->setSessionToken(m_api->sessionToken());
    // The cursor provider is re-evaluated on every (re)connect so the stream
    // resumes after the latest durable message.
    m_stream->setAfterIdProvider([this]() { return m_session->afterId(); });

    connect(m_stream, &ChatStreamWebSocket::eventsReceived, m_session, &ChatSession::applyEvents);
    connect(m_stream, &ChatStreamWebSocket::streamStateChanged, this,
            &ChatController::connectionStateChanged);

    connect(m_session, &ChatSession::statusChanged, this,
            [this](ChatStatus, ChatStatus) { emit statusChanged(); });
    connect(m_session, &ChatSession::queueChanged, this, &ChatController::queueChanged);
    connect(m_session, &ChatSession::errorChanged, this, &ChatController::errorChanged);
    connect(m_session, &ChatSession::retryChanged, this, &ChatController::retryChanged);
    connect(m_session, &ChatSession::actionRequiredChanged, this,
            &ChatController::actionRequiredChanged);

    connect(m_model, &ChatMessagesModel::loadOlderRequested, this, [this](qint64 beforeId) {
        m_api->listMessages(m_chatId, beforeId, 0, kMessagePageSize);
    });

    connect(m_api, &AgentsApiClient::messagesReceived, this,
            [this](const QString& chatId, const QList<ChatMessage>& messages,
                   const QList<ChatQueuedMessage>& queued, bool hasMore, qint64 beforeId,
                   qint64 afterId) {
                Q_UNUSED(afterId);
                if (chatId != m_chatId) return;
                if (beforeId > 0) {
                    // Older page requested through loadOlder().
                    m_session->prependOlderMessages(messages);
                    m_model->finishLoadOlder(hasMore);
                    return;
                }
                if (m_initialLoaded) return;
                m_initialLoaded = true;
                m_session->setInitialMessages(messages);
                m_session->setQueuedMessages(queued);
                m_model->setHasMore(hasMore);
            });

    connect(m_api, &AgentsApiClient::usageLimitExceeded, this,
            [this](const QString& chatId, const ChatUsageLimitExceeded& payload) {
                if (chatId != m_chatId) return;
                m_hasUsageLimit = true;
                m_usageLimit = payload;
                emit usageLimitChanged();
                emit usageLimitExceeded(payload);
            });

    connect(m_api, &AgentsApiClient::messageSent, this,
            [this](const QString& chatId, bool, const QJsonObject&) {
                if (chatId != m_chatId) return;
                clearUsageLimit();
                emit messageSent();
            });

    connect(m_api, &AgentsApiClient::chatReceived, this, [this](const Chat& chat) {
        if (chat.id == m_chatId) applyChatInfo(chat);
    });
    connect(m_api, &AgentsApiClient::chatUpdated, this, [this](const QString& chatId) {
        if (chatId == m_chatId) refreshChat();
    });

    connect(m_api, &AgentsApiClient::promptsReceived, this,
            [this](const QString& chatId, const QStringList& prompts) {
                if (chatId != m_chatId) return;
                m_prompts = prompts;
                emit promptsChanged();
            });

    connect(m_api, &AgentsApiClient::diffReceived, this,
            [this](const QString& chatId, const QJsonObject& diff) {
                if (chatId != m_chatId) return;
                m_diffLoading = false;
                m_pullRequestUrl = diff.value(QLatin1String("pull_request_url")).toString();
                m_diffFiles = DiffParser::toVariantList(
                    DiffParser::parse(diff.value(QLatin1String("diff")).toString()));
                emit diffChanged();
            });
}

void ChatController::setDraft(const QString& draft) {
    if (m_draft == draft) return;
    m_draft = draft;
    persistDraft();
    emit draftChanged();
}

void ChatController::loadPersistedDraft() {
    QSettings settings;
    m_draft = settings.value(kDraftSettingsGroup + QLatin1Char('/') + m_chatId).toString();
}

void ChatController::persistDraft() {
    // Drafts survive app restarts; empty drafts remove the stored key.
    QSettings settings;
    const QString key = kDraftSettingsGroup + QLatin1Char('/') + m_chatId;
    if (m_draft.isEmpty())
        settings.remove(key);
    else
        settings.setValue(key, m_draft);
}

void ChatController::start() {
    m_api->listMessages(m_chatId, 0, 0, kMessagePageSize);
    m_api->getChat(m_chatId);
    m_api->getPrompts(m_chatId);
    m_stream->openStream(m_chatId);
}

void ChatController::applyChatInfo(const Chat& chat) {
    m_chat = chat;
    emit chatInfoChanged();
}

QVariantList ChatController::queuedMessagesVariant() const {
    QVariantList list;
    for (const ChatQueuedMessage& q : m_session->queuedMessages()) {
        QString text;
        for (const ChatMessagePart& p : q.content) {
            if (p.type == ChatMessagePartType::Text) {
                text = p.text;
                break;
            }
        }
        QVariantMap m;
        m.insert(QStringLiteral("queuedId"), q.id);
        m.insert(QStringLiteral("text"), text);
        m.insert(QStringLiteral("createdAt"), q.createdAt);
        list.append(m);
    }
    return list;
}

bool ChatController::hasActionRequired() const {
    return m_session->hasActionRequired() && !m_session->actionRequired().toolCalls.isEmpty();
}

QVariantList ChatController::actionToolCalls() const {
    QVariantList list;
    if (!m_session->hasActionRequired()) return list;
    for (const ChatStreamToolCall& t : m_session->actionRequired().toolCalls) {
        QVariantMap m;
        m.insert(QStringLiteral("toolCallId"), t.toolCallId);
        m.insert(QStringLiteral("toolName"), t.toolName);
        m.insert(QStringLiteral("argsJson"), t.args);
        list.append(m);
    }
    return list;
}

QVariantList ChatController::parsePlanSteps(const QString& markdown) const {
    return PlanStepParser::toVariantList(PlanStepParser::parse(markdown));
}

QVariantMap ChatController::parseToolSchema(const QString& schemaJson) const {
    QVariantMap out;
    const QJsonDocument doc = QJsonDocument::fromJson(schemaJson.toUtf8());
    bool ok = false;
    const QVariantList fields =
        doc.isObject() ? JsonSchemaParser::parse(doc.object(), &ok) : QVariantList{};
    out.insert(QStringLiteral("supported"), ok);
    out.insert(QStringLiteral("fields"), fields);
    return out;
}

void ChatController::sendMessage(const QString& text, const QString& busyBehavior) {
    if (text.trimmed().isEmpty()) return;
    QJsonObject part;
    part.insert(QLatin1String("type"), QStringLiteral("text"));
    part.insert(QLatin1String("text"), text);
    m_api->sendMessage(m_chatId, QJsonArray{part}, busyBehavior);
}

void ChatController::sendMessageWithOptions(const QString& text, const QVariantList& attachments,
                                            const QString& busyBehavior,
                                            const QVariantMap& options) {
    QJsonArray content;
    if (!text.trimmed().isEmpty()) {
        QJsonObject part;
        part.insert(QLatin1String("type"), QStringLiteral("text"));
        part.insert(QLatin1String("text"), text);
        content.append(part);
    }
    for (const QVariant& v : attachments) {
        const QVariantMap a = v.toMap();
        QJsonObject part;
        part.insert(QLatin1String("type"), QStringLiteral("file"));
        part.insert(QLatin1String("file_id"), a.value(QStringLiteral("fileId")).toString());
        part.insert(QLatin1String("name"), a.value(QStringLiteral("name")).toString());
        part.insert(QLatin1String("media_type"), a.value(QStringLiteral("mediaType")).toString());
        content.append(part);
    }
    if (content.isEmpty()) return;

    QJsonObject extra;
    const QString modelConfigId = options.value(QStringLiteral("modelConfigId")).toString();
    if (!modelConfigId.isEmpty()) extra.insert(QLatin1String("model_config_id"), modelConfigId);
    const QStringList mcpServerIds = options.value(QStringLiteral("mcpServerIds")).toStringList();
    if (!mcpServerIds.isEmpty()) {
        QJsonArray ids;
        for (const QString& id : mcpServerIds) ids.append(id);
        extra.insert(QLatin1String("mcp_server_ids"), ids);
    }
    if (options.contains(QStringLiteral("planMode"))) {
        extra.insert(QLatin1String("plan_mode"), options.value(QStringLiteral("planMode")).toBool()
                                                     ? QStringLiteral("plan")
                                                     : QString());
    }
    m_api->sendMessage(m_chatId, content, busyBehavior, extra);
}

void ChatController::interrupt() {
    m_api->interrupt(m_chatId);
}

void ChatController::loadOlder() {
    m_model->loadOlder();
}

void ChatController::promoteQueued(qint64 queuedId) {
    m_api->promoteQueued(m_chatId, queuedId);
}

void ChatController::deleteQueued(qint64 queuedId) {
    m_api->deleteQueued(m_chatId, queuedId);
}

void ChatController::submitToolResults(const QJsonArray& results) {
    m_api->sendToolResults(m_chatId, results);
}

void ChatController::submitToolResult(const QString& toolCallId, const QString& outputJson,
                                      bool isError) {
    QJsonObject result;
    result.insert(QLatin1String("tool_call_id"), toolCallId);
    result.insert(QLatin1String("output"), parseJsonValue(outputJson));
    result.insert(QLatin1String("is_error"), isError);
    m_api->sendToolResults(m_chatId, QJsonArray{result});
}

void ChatController::reconnect() {
    m_stream->reconnectNow();
}

void ChatController::refreshChat() {
    m_api->getChat(m_chatId);
}

void ChatController::refreshMessages() {
    m_initialLoaded = false;
    m_api->listMessages(m_chatId, 0, 0, kMessagePageSize);
}

void ChatController::rename(const QString& title) {
    QJsonObject patch;
    patch.insert(QLatin1String("title"), title);
    m_api->updateChat(m_chatId, patch);
}

void ChatController::setArchived(bool archived) {
    QJsonObject patch;
    patch.insert(QLatin1String("archived"), archived);
    m_api->updateChat(m_chatId, patch);
}

void ChatController::setPlanModeEnabled(bool enabled) {
    QJsonObject patch;
    patch.insert(QLatin1String("plan_mode"), enabled ? QStringLiteral("plan") : QString());
    m_api->updateChat(m_chatId, patch);
}

void ChatController::regenerateTitle() {
    m_api->regenerateTitle(m_chatId);
}

void ChatController::fetchPrompts() {
    m_api->getPrompts(m_chatId);
}

void ChatController::fetchDiff() {
    if (m_diffLoading) return;
    m_diffLoading = true;
    emit diffChanged();
    m_api->getDiff(m_chatId);
}

void ChatController::implementPlan() {
    // Matches Android's implementActivePlan(): clear the persistent plan
    // mode, then post the canned kickoff message in execute mode.
    setPlanModeEnabled(false);
    sendMessage(QStringLiteral("Implement the plan above."));
}

void ChatController::clearUsageLimit() {
    if (!m_hasUsageLimit) return;
    m_hasUsageLimit = false;
    emit usageLimitChanged();
}

// ===========================================================================
// AgentsController
// ===========================================================================

AgentsController::AgentsController(AgentsApiClient* api, QObject* parent)
    : QObject(parent), m_api(api), m_watch(new ChatWatchWebSocket(this)) {
    m_pollTimer.setInterval(kPollIntervalMs);
    loadUiPrefs();

    connect(m_watch, &ChatWatchWebSocket::watchEventReceived, this,
            &AgentsController::handleWatchEvent);
    connect(m_watch, &ChatWatchWebSocket::watchStateChanged, this,
            &AgentsController::updatePollingState);

    connect(&m_pollTimer, &QTimer::timeout, this, [this]() { m_api->listChats(); });

    connect(m_api, &AgentsApiClient::chatsReceived, this, [this](const QList<Chat>& chats) {
        m_endpointOk = true;
        m_daemonMissing = false;
        setChatList(chats);
        updateAvailability();
        // Later polled lists diff against this baseline for notifications.
        m_firstFetch = false;
    });
    connect(m_api, &AgentsApiClient::chatsJsonReceived, this, &AgentsController::saveCache);

    connect(m_api, &AgentsApiClient::chatCreated, this, [this](const Chat& chat) {
        applyUpsert(chat);
        recomputeCounts();
        emit chatCreated(chat.id);
    });
    // Refresh the list row after PATCH operations complete.
    connect(m_api, &AgentsApiClient::chatUpdated, this,
            [this](const QString& chatId) { m_api->getChat(chatId); });
    connect(m_api, &AgentsApiClient::chatReceived, this, [this](const Chat& chat) {
        applyUpsert(chat);
        recomputeCounts();
    });

    connect(m_api, &AgentsApiClient::createChatPermissionReceived, this, [this](bool allowed) {
        m_canCreate = allowed;
        updateAvailability();
    });
    connect(m_api, &AgentsApiClient::modelsReceived, this,
            [this](const ChatModelsResponse& models) {
                m_modelsAvailable = models.anyAvailable();
                updateAvailability();
            });
    connect(m_api, &AgentsApiClient::modelConfigsReceived, this,
            [this](const QList<ChatModelConfig>& configs) {
                m_modelConfigs = configs;
                emit configsChanged();
            });
    connect(m_api, &AgentsApiClient::mcpServersReceived, this,
            [this](const QList<McpServerConfig>& servers) {
                m_mcpServers = servers;
                emit configsChanged();
            });
    connect(m_api, &AgentsApiClient::organizationsReceived, this, [this](const QJsonArray& orgs) {
        QString id;
        for (const QJsonValue& v : orgs) {
            const QJsonObject o = v.toObject();
            if (id.isEmpty()) id = o.value(QLatin1String("id")).toString();
            if (o.value(QLatin1String("is_default")).toBool()) {
                id = o.value(QLatin1String("id")).toString();
                break;
            }
        }
        if (id == m_organizationId) return;
        m_organizationId = id;
        emit organizationChanged();
    });

    connect(m_api, &AgentsApiClient::fileUploaded, this, [this](const QString& fileId) {
        if (!m_uploadInFlight || m_uploadQueue.isEmpty()) return;
        const PendingUpload done = m_uploadQueue.dequeue();
        m_uploadInFlight = false;
        emit attachmentUploaded(done.localPath, fileId, done.name, done.mediaType);
        startNextUpload();
    });

    connect(m_api, &AgentsApiClient::requestFailed, this,
            [this](const QString& endpoint, int statusCode, const QString& errorMessage,
                   const QByteArray& body) {
                if (m_uploadInFlight && endpoint.contains(QLatin1String("/files?"))) {
                    const PendingUpload failed = m_uploadQueue.dequeue();
                    m_uploadInFlight = false;
                    emit attachmentUploadFailed(failed.localPath, errorMessage);
                    startNextUpload();
                    return;
                }
                if (!endpoint.startsWith(QLatin1String("/api/experimental/chats"))) return;
                // A deployment without a configured chat daemon answers 500
                // with a distinctive message body.
                if (statusCode == 500 && body.contains("chat daemon not configured")) {
                    m_daemonMissing = true;
                    m_endpointOk = false;
                    updateAvailability();
                }
            });
}

void AgentsController::start() {
    m_started = true;
    m_firstFetch = true;
    loadCache();

    // Availability probe: permission check, endpoint reachability, and model
    // availability all feed the available/modelsAvailable properties.
    m_api->checkCreateChatPermission();
    m_api->listChats();
    m_api->listModels();
    m_api->listModelConfigs();
    m_api->listMcpServers();
    m_api->listOrganizations();

    m_watch->setBaseUrl(m_api->baseUrl());
    m_watch->setSessionToken(m_api->sessionToken());
    m_watch->openWatch();
    updatePollingState();
}

void AgentsController::stop() {
    m_started = false;
    m_watch->closeWatch();
    m_pollTimer.stop();

    // Tear down every open chat so its stream socket closes and cannot
    // reconnect against a switched deployment.
    const QList<ChatController*> open =
        findChildren<ChatController*>(QString(), Qt::FindDirectChildrenOnly);
    for (ChatController* c : open) c->deleteLater();

    setFocusedChatId(QString());

    // Clear the chat list and counts; tray and tab badges reset through
    // chatsReset/countsChanged. m_firstFetch also suppresses notifications
    // for the next deployment's initial load.
    m_firstFetch = true;
    setChatList({});

    // Reset the availability probe so the next start() re-derives it
    // against the new deployment instead of showing stale state.
    m_canCreate = false;
    m_endpointOk = false;
    m_daemonMissing = false;
    m_modelsAvailable = false;
    updateAvailability();

    m_modelConfigs.clear();
    m_mcpServers.clear();
    emit configsChanged();

    emit stopped();
}

void AgentsController::refreshNow() {
    m_api->listChats();
}

ChatController* AgentsController::openChat(const QString& chatId) {
    auto* controller = new ChatController(m_api, chatId, this);
    controller->start();
    return controller;
}

QVariantList AgentsController::subagentsOf(const QString& chatId) const {
    QVariantList list;
    for (const Chat& c : m_chats) {
        if (c.id != chatId) continue;
        for (const Chat& child : c.children) {
            QVariantMap m;
            m.insert(QStringLiteral("id"), child.id);
            m.insert(QStringLiteral("title"), child.title);
            m.insert(QStringLiteral("statusString"), child.statusString);
            list.append(m);
        }
        break;
    }
    return list;
}

// ---------------------------------------------------------------------------
// Create flow / list actions
// ---------------------------------------------------------------------------

QVariantList AgentsController::modelConfigs() const {
    QVariantList list;
    for (const ChatModelConfig& c : m_modelConfigs) {
        if (!c.enabled) continue;
        QVariantMap m;
        m.insert(QStringLiteral("id"), c.id);
        m.insert(QStringLiteral("displayName"), c.displayName.isEmpty() ? c.model : c.displayName);
        m.insert(QStringLiteral("model"), c.model);
        m.insert(QStringLiteral("isDefault"), c.isDefault);
        list.append(m);
    }
    return list;
}

QString AgentsController::defaultModelConfigId() const {
    for (const ChatModelConfig& c : m_modelConfigs) {
        if (c.enabled && c.isDefault) return c.id;
    }
    for (const ChatModelConfig& c : m_modelConfigs) {
        if (c.enabled) return c.id;
    }
    return {};
}

QVariantList AgentsController::mcpServers() const {
    QVariantList list;
    for (const McpServerConfig& s : m_mcpServers) {
        QVariantMap m;
        m.insert(QStringLiteral("id"), s.id);
        m.insert(QStringLiteral("displayName"), s.displayName.isEmpty() ? s.slug : s.displayName);
        m.insert(QStringLiteral("availability"), s.availability);
        m.insert(QStringLiteral("forceOn"), s.availability == QLatin1String("force_on"));
        m.insert(QStringLiteral("defaultOn"), s.availability == QLatin1String("default_on"));
        m.insert(QStringLiteral("allowInPlanMode"), s.allowInPlanMode);
        list.append(m);
    }
    return list;
}

void AgentsController::createChat(const QString& prompt, const QString& workspaceId,
                                  const QString& modelConfigId, const QString& reasoningEffort,
                                  const QStringList& mcpServerIds, bool planMode,
                                  const QVariantList& attachments) {
    QJsonArray content;
    if (!prompt.trimmed().isEmpty()) {
        QJsonObject part;
        part.insert(QLatin1String("type"), QStringLiteral("text"));
        part.insert(QLatin1String("text"), prompt);
        content.append(part);
    }
    for (const QVariant& v : attachments) {
        const QVariantMap a = v.toMap();
        QJsonObject part;
        part.insert(QLatin1String("type"), QStringLiteral("file"));
        part.insert(QLatin1String("file_id"), a.value(QStringLiteral("fileId")).toString());
        part.insert(QLatin1String("name"), a.value(QStringLiteral("name")).toString());
        part.insert(QLatin1String("media_type"), a.value(QStringLiteral("mediaType")).toString());
        content.append(part);
    }

    QJsonObject req;
    req.insert(QLatin1String("organization_id"), m_organizationId);
    req.insert(QLatin1String("content"), content);
    req.insert(QLatin1String("client_type"), QStringLiteral("ui"));
    if (!workspaceId.isEmpty()) req.insert(QLatin1String("workspace_id"), workspaceId);
    if (!modelConfigId.isEmpty()) req.insert(QLatin1String("model_config_id"), modelConfigId);
    if (!reasoningEffort.isEmpty()) req.insert(QLatin1String("reasoning_effort"), reasoningEffort);
    if (!mcpServerIds.isEmpty()) {
        QJsonArray ids;
        for (const QString& id : mcpServerIds) ids.append(id);
        req.insert(QLatin1String("mcp_server_ids"), ids);
    }
    if (planMode) req.insert(QLatin1String("plan_mode"), QStringLiteral("plan"));
    m_api->createChat(req);
}

void AgentsController::pinChat(const QString& chatId, bool pinned) {
    int order = 0;
    if (pinned) {
        for (const Chat& c : m_chats) order = qMax(order, c.pinOrder);
        ++order;
    }
    QJsonObject patch;
    patch.insert(QLatin1String("pin_order"), order);
    m_api->updateChat(chatId, patch);
}

void AgentsController::archiveChat(const QString& chatId, bool archived) {
    QJsonObject patch;
    patch.insert(QLatin1String("archived"), archived);
    m_api->updateChat(chatId, patch);
}

void AgentsController::renameChat(const QString& chatId, const QString& title) {
    QJsonObject patch;
    patch.insert(QLatin1String("title"), title);
    m_api->updateChat(chatId, patch);
}

void AgentsController::regenerateChatTitle(const QString& chatId) {
    m_api->regenerateTitle(chatId);
}

// ---------------------------------------------------------------------------
// Attachment uploads (serialized so replies correlate to requests)
// ---------------------------------------------------------------------------

void AgentsController::uploadAttachment(const QUrl& fileUrl) {
    const QString localPath = fileUrl.isLocalFile() ? fileUrl.toLocalFile() : fileUrl.toString();
    const QFileInfo info(localPath);
    PendingUpload upload;
    upload.localPath = localPath;
    upload.name = info.fileName();
    upload.mediaType = QMimeDatabase().mimeTypeForFile(info).name();
    m_uploadQueue.enqueue(upload);
    startNextUpload();
}

void AgentsController::startNextUpload() {
    if (m_uploadInFlight || m_uploadQueue.isEmpty()) return;
    const PendingUpload& next = m_uploadQueue.head();
    QFile f(next.localPath);
    if (!f.open(QIODevice::ReadOnly)) {
        const PendingUpload failed = m_uploadQueue.dequeue();
        emit attachmentUploadFailed(failed.localPath, QStringLiteral("could not read file"));
        startNextUpload();
        return;
    }
    m_uploadInFlight = true;
    m_api->uploadFile(m_organizationId, next.name, next.mediaType, f.readAll());
}

// ---------------------------------------------------------------------------
// Persisted UI preferences
// ---------------------------------------------------------------------------

void AgentsController::loadUiPrefs() {
    QSettings settings;
    m_lastWorkspaceId = settings.value(kPrefLastWorkspace).toString();
    m_lastModelConfigId = settings.value(kPrefLastModelConfig).toString();
    m_sendShortcut = settings.value(kPrefSendShortcut, QStringLiteral("enter")).toString();
}

void AgentsController::setLastWorkspaceId(const QString& id) {
    if (m_lastWorkspaceId == id) return;
    m_lastWorkspaceId = id;
    QSettings().setValue(kPrefLastWorkspace, id);
    emit uiPrefsChanged();
}

void AgentsController::setLastModelConfigId(const QString& id) {
    if (m_lastModelConfigId == id) return;
    m_lastModelConfigId = id;
    QSettings().setValue(kPrefLastModelConfig, id);
    emit uiPrefsChanged();
}

void AgentsController::setSendShortcut(const QString& shortcut) {
    if (m_sendShortcut == shortcut) return;
    m_sendShortcut = shortcut;
    QSettings().setValue(kPrefSendShortcut, shortcut);
    emit uiPrefsChanged();
}

void AgentsController::setFocusedChatId(const QString& chatId) {
    if (m_focusedChatId == chatId) return;
    m_focusedChatId = chatId;
    emit focusedChatIdChanged();
}

// ---------------------------------------------------------------------------
// Watch events and chat list state
// ---------------------------------------------------------------------------

ChatStatus AgentsController::knownStatusOf(const QString& chatId) const {
    for (const Chat& c : m_chats) {
        if (c.id == chatId) return c.status;
        for (const Chat& child : c.children) {
            if (child.id == chatId) return child.status;
        }
    }
    return ChatStatus::Unknown;
}

void AgentsController::handleWatchEvent(const ChatWatchEvent& event) {
    if (event.kind == QLatin1String("deleted")) {
        const QString id = event.chat.id;
        for (int i = 0; i < m_chats.size(); ++i) {
            if (m_chats.at(i).id == id) {
                m_chats.removeAt(i);
                emit chatRemoved(id);
                recomputeCounts();
                return;
            }
            for (int j = 0; j < m_chats[i].children.size(); ++j) {
                if (m_chats[i].children.at(j).id == id) {
                    m_chats[i].children.removeAt(j);
                    emit chatRemoved(id);
                    recomputeCounts();
                    return;
                }
            }
        }
        return;
    }

    const ChatStatus oldStatus = knownStatusOf(event.chat.id);
    applyUpsert(event.chat);

    if (event.kind == QLatin1String("status_change") && oldStatus != event.chat.status)
        emit chatStatusChanged(event.chat, oldStatus, event.chat.status);
    if (event.kind == QLatin1String("action_required")) emit actionRequired(event.chat);

    recomputeCounts();
}

void AgentsController::applyUpsert(const Chat& chat) {
    if (chat.isSubagent()) {
        for (Chat& parent : m_chats) {
            if (parent.id != chat.parentChatId) continue;
            for (Chat& child : parent.children) {
                if (child.id == chat.id) {
                    child = chat;
                    emit chatUpserted(chat);
                    return;
                }
            }
            parent.children.append(chat);
            emit chatUpserted(chat);
            return;
        }
        // Parent not known yet; refresh restores consistency.
        m_api->listChats();
        return;
    }
    for (Chat& existing : m_chats) {
        if (existing.id != chat.id) continue;
        Chat merged = chat;
        // Watch payloads may omit children; keep the known ones.
        if (merged.children.isEmpty()) merged.children = existing.children;
        existing = merged;
        emit chatUpserted(merged);
        return;
    }
    m_chats.append(chat);
    emit chatUpserted(chat);
}

void AgentsController::setChatList(const QList<Chat>& chats) {
    // Diff statuses against the previous list so the polling fallback
    // produces the same notification signals as the watch socket. The
    // first load after start() is suppressed (m_firstFetch).
    struct Transition {
        Chat chat;
        ChatStatus oldStatus;
    };
    QList<Transition> statusChanges;
    if (!m_firstFetch) {
        QHash<QString, ChatStatus> oldStatus;
        for (const Chat& c : m_chats) {
            oldStatus.insert(c.id, c.status);
            for (const Chat& child : c.children) oldStatus.insert(child.id, child.status);
        }
        const auto collect = [&](const Chat& c) {
            const auto it = oldStatus.constFind(c.id);
            if (it == oldStatus.constEnd()) return;  // brand-new chat, no baseline
            if (it.value() != c.status) statusChanges.append({c, it.value()});
        };
        for (const Chat& c : chats) {
            collect(c);
            for (const Chat& child : c.children) collect(child);
        }
    }

    m_chats = chats;
    emit chatsReset(m_chats);

    for (const Transition& t : statusChanges) {
        emit chatStatusChanged(t.chat, t.oldStatus, t.chat.status);
        if (t.chat.status == ChatStatus::RequiresAction) emit actionRequired(t.chat);
    }

    recomputeCounts();
}

void AgentsController::recomputeCounts() {
    int unread = 0;
    int running = 0;
    int action = 0;
    for (const Chat& c : m_chats) {
        if (c.archived) continue;
        if (c.hasUnread) ++unread;
        if (c.status == ChatStatus::Running || c.status == ChatStatus::Pending) ++running;
        if (c.status == ChatStatus::RequiresAction) ++action;
    }
    if (unread == m_unreadCount && running == m_runningCount && action == m_requiresActionCount)
        return;
    m_unreadCount = unread;
    m_runningCount = running;
    m_requiresActionCount = action;
    emit countsChanged();
}

// ---------------------------------------------------------------------------
// Availability
// ---------------------------------------------------------------------------

void AgentsController::updateAvailability() {
    const bool available = m_canCreate && m_endpointOk && !m_daemonMissing;
    QString reason;
    if (m_daemonMissing) {
        reason = QStringLiteral("chat daemon not configured");
    } else if (!m_canCreate) {
        reason = QStringLiteral("missing chat create permission");
    } else if (!m_endpointOk) {
        reason = QStringLiteral("chats endpoint unreachable");
    }
    if (available == m_available && reason == m_availabilityReason) {
        // modelsAvailable shares this notify signal and may have changed.
        emit availabilityChanged();
        return;
    }
    m_available = available;
    m_availabilityReason = reason;
    emit availabilityChanged();
}

// ---------------------------------------------------------------------------
// Polling fallback
// ---------------------------------------------------------------------------

void AgentsController::updatePollingState() {
    if (!m_started) {
        m_pollTimer.stop();
        return;
    }
    // Polling covers the gaps: no compiled-in WebSocket support, or a watch
    // socket that is not currently open.
    const bool needPolling =
        !ChatWatchWebSocket::websocketsAvailable() ||
        m_watch->connectionState() != ChatWatchWebSocket::ConnectionState::Open;
    if (needPolling && !m_pollTimer.isActive()) {
        qCDebug(lcAgents) << "watch socket unhealthy, starting chat list polling";
        m_pollTimer.start();
    } else if (!needPolling && m_pollTimer.isActive()) {
        qCDebug(lcAgents) << "watch socket healthy, stopping chat list polling";
        m_pollTimer.stop();
    }
}

// ---------------------------------------------------------------------------
// Per-deployment disk cache
// ---------------------------------------------------------------------------

QString AgentsController::cacheDirForDeployment() const {
    const auto cacheRoot = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    const auto urlHash = QString::fromLatin1(
        QCryptographicHash::hash(m_api->baseUrl().toUtf8(), QCryptographicHash::Sha1).toHex());
    const auto dir = cacheRoot + QLatin1Char('/') + urlHash;
    QDir().mkpath(dir);
    return dir;
}

void AgentsController::loadCache() {
    QFile f(cacheDirForDeployment() + QStringLiteral("/agent_chats.json"));
    if (!f.open(QIODevice::ReadOnly)) return;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isArray()) return;
    setChatList(Chat::listFromJson(doc.array()));
    qCDebug(lcAgents) << "loaded" << m_chats.size() << "chats from cache";
}

void AgentsController::saveCache(const QJsonArray& raw) {
    QSaveFile f(cacheDirForDeployment() + QStringLiteral("/agent_chats.json"));
    if (!f.open(QIODevice::WriteOnly)) return;
    f.write(QJsonDocument(raw).toJson(QJsonDocument::Compact));
    f.commit();
}
