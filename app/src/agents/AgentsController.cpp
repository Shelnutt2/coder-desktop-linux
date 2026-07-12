#include "agents/AgentsController.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QSaveFile>
#include <QStandardPaths>

Q_LOGGING_CATEGORY(lcAgents, "coder.agents.controller")

// ===========================================================================
// ChatController
// ===========================================================================

ChatController::ChatController(AgentsApiClient* api, const QString& chatId, QObject* parent)
    : QObject(parent), m_api(api), m_chatId(chatId) {
    m_session = new ChatSession(chatId, this);
    m_stream = new ChatStreamWebSocket(this);
    m_model = new ChatMessagesModel(m_session, this);

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
                if (chatId == m_chatId) emit usageLimitExceeded(payload);
            });
}

void ChatController::setDraft(const QString& draft) {
    if (m_draft == draft) return;
    m_draft = draft;
    emit draftChanged();
}

void ChatController::start() {
    m_api->listMessages(m_chatId, 0, 0, kMessagePageSize);
    m_stream->openStream(m_chatId);
}

void ChatController::sendMessage(const QString& text, const QString& busyBehavior) {
    if (text.trimmed().isEmpty()) return;
    QJsonObject part;
    part.insert(QLatin1String("type"), QStringLiteral("text"));
    part.insert(QLatin1String("text"), text);
    m_api->sendMessage(m_chatId, QJsonArray{part}, busyBehavior);
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

// ===========================================================================
// AgentsController
// ===========================================================================

AgentsController::AgentsController(AgentsApiClient* api, QObject* parent)
    : QObject(parent), m_api(api), m_watch(new ChatWatchWebSocket(this)) {
    m_pollTimer.setInterval(kPollIntervalMs);

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
    });
    connect(m_api, &AgentsApiClient::chatsJsonReceived, this, &AgentsController::saveCache);

    connect(m_api, &AgentsApiClient::createChatPermissionReceived, this, [this](bool allowed) {
        m_canCreate = allowed;
        updateAvailability();
    });
    connect(m_api, &AgentsApiClient::modelsReceived, this,
            [this](const ChatModelsResponse& models) {
                m_modelsAvailable = models.anyAvailable();
                updateAvailability();
            });
    connect(
        m_api, &AgentsApiClient::requestFailed, this,
        [this](const QString& endpoint, int statusCode, const QString&, const QByteArray& body) {
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
    loadCache();

    // Availability probe: permission check, endpoint reachability, and model
    // availability all feed the available/modelsAvailable properties.
    m_api->checkCreateChatPermission();
    m_api->listChats();
    m_api->listModels();

    m_watch->setBaseUrl(m_api->baseUrl());
    m_watch->setSessionToken(m_api->sessionToken());
    m_watch->openWatch();
    updatePollingState();
}

void AgentsController::stop() {
    m_started = false;
    m_watch->closeWatch();
    m_pollTimer.stop();
}

void AgentsController::refreshNow() {
    m_api->listChats();
}

ChatController* AgentsController::openChat(const QString& chatId) {
    auto* controller = new ChatController(m_api, chatId, this);
    controller->start();
    return controller;
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
    m_chats = chats;
    emit chatsReset(m_chats);
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
