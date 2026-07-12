#ifndef AGENTSCONTROLLER_H
#define AGENTSCONTROLLER_H

#include <QDateTime>
#include <QHash>
#include <QJsonArray>
#include <QList>
#include <QObject>
#include <QQueue>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>

#include "agents/ChatSession.h"
#include "api/AgentsApiClient.h"
#include "api/ChatStreamWebSocket.h"
#include "api/ChatWatchWebSocket.h"
#include "api/dto/Chat.h"
#include "api/dto/ChatModels.h"
#include "api/dto/ChatStreamEvent.h"
#include "models/ChatMessagesModel.h"

class AgentsController;

/// Per-open-chat facade returned by AgentsController::openChat().
///
/// Owns the chat's ChatSession, ChatStreamWebSocket, and ChatMessagesModel,
/// wires stream events into the session, and exposes QML-friendly slots and
/// properties for the chat view. Destroy (deleteLater) to close the chat.
class ChatController : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString chatId READ chatId CONSTANT)
    Q_PROPERTY(QObject* messagesModel READ messagesModel CONSTANT)
    Q_PROPERTY(int status READ status NOTIFY statusChanged)
    Q_PROPERTY(QString statusString READ statusString NOTIFY statusChanged)
    Q_PROPERTY(int queuedCount READ queuedCount NOTIFY queueChanged)
    Q_PROPERTY(QVariantList queuedMessages READ queuedMessagesVariant NOTIFY queueChanged)
    Q_PROPERTY(int connectionState READ connectionState NOTIFY connectionStateChanged)
    Q_PROPERTY(QString draft READ draft WRITE setDraft NOTIFY draftChanged)

    // Chat metadata refreshed via GET /chats/{id} and after PATCH updates.
    Q_PROPERTY(QString title READ title NOTIFY chatInfoChanged)
    Q_PROPERTY(bool archived READ archived NOTIFY chatInfoChanged)
    Q_PROPERTY(bool planMode READ planMode NOTIFY chatInfoChanged)
    Q_PROPERTY(QString workspaceId READ workspaceId NOTIFY chatInfoChanged)
    Q_PROPERTY(QString parentChatId READ parentChatId NOTIFY chatInfoChanged)

    // Live stream state surfaced from the ChatSession.
    Q_PROPERTY(bool hasError READ hasError NOTIFY errorChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorChanged)
    Q_PROPERTY(QString errorKind READ errorKind NOTIFY errorChanged)
    Q_PROPERTY(bool errorRetryable READ errorRetryable NOTIFY errorChanged)
    Q_PROPERTY(bool hasRetry READ hasRetry NOTIFY retryChanged)
    Q_PROPERTY(int retryAttempt READ retryAttempt NOTIFY retryChanged)
    Q_PROPERTY(int retryDelayMs READ retryDelayMs NOTIFY retryChanged)
    Q_PROPERTY(bool hasActionRequired READ hasActionRequired NOTIFY actionRequiredChanged)
    Q_PROPERTY(QVariantList actionToolCalls READ actionToolCalls NOTIFY actionRequiredChanged)

    // Usage-limit banner state (HTTP 409 from POST messages).
    Q_PROPERTY(bool hasUsageLimit READ hasUsageLimit NOTIFY usageLimitChanged)
    Q_PROPERTY(double usageSpentMicros READ usageSpentMicros NOTIFY usageLimitChanged)
    Q_PROPERTY(double usageLimitMicros READ usageLimitMicros NOTIFY usageLimitChanged)
    Q_PROPERTY(QDateTime usageResetsAt READ usageResetsAt NOTIFY usageLimitChanged)

    // Composer history (GET /chats/{id}/prompts, newest first).
    Q_PROPERTY(QStringList prompts READ prompts NOTIFY promptsChanged)

    // Diff viewer state (GET /chats/{id}/diff parsed through DiffParser).
    Q_PROPERTY(QVariantList diffFiles READ diffFiles NOTIFY diffChanged)
    Q_PROPERTY(QString pullRequestUrl READ pullRequestUrl NOTIFY diffChanged)
    Q_PROPERTY(bool diffLoading READ diffLoading NOTIFY diffChanged)

public:
    /// api must outlive this controller.
    explicit ChatController(AgentsApiClient* api, const QString& chatId, QObject* parent = nullptr);

    [[nodiscard]] QString chatId() const { return m_chatId; }
    [[nodiscard]] QObject* messagesModel() const { return m_model; }
    [[nodiscard]] ChatSession* session() const { return m_session; }
    [[nodiscard]] int status() const { return static_cast<int>(m_session->status()); }
    [[nodiscard]] QString statusString() const { return chatStatusToString(m_session->status()); }
    [[nodiscard]] int queuedCount() const {
        return static_cast<int>(m_session->queuedMessages().size());
    }
    [[nodiscard]] QVariantList queuedMessagesVariant() const;
    [[nodiscard]] int connectionState() const {
        return static_cast<int>(m_stream->connectionState());
    }
    [[nodiscard]] QString draft() const { return m_draft; }
    void setDraft(const QString& draft);

    [[nodiscard]] QString title() const { return m_chat.title; }
    [[nodiscard]] bool archived() const { return m_chat.archived; }
    [[nodiscard]] bool planMode() const { return m_chat.planMode == QLatin1String("plan"); }
    [[nodiscard]] QString workspaceId() const { return m_chat.workspaceId; }
    [[nodiscard]] QString parentChatId() const { return m_chat.parentChatId; }

    [[nodiscard]] bool hasError() const { return m_session->hasError(); }
    [[nodiscard]] QString errorMessage() const { return m_session->lastError().message; }
    [[nodiscard]] QString errorKind() const { return m_session->lastError().kind; }
    [[nodiscard]] bool errorRetryable() const { return m_session->lastError().retryable; }
    [[nodiscard]] bool hasRetry() const { return m_session->hasRetry(); }
    [[nodiscard]] int retryAttempt() const { return m_session->retry().attempt; }
    [[nodiscard]] int retryDelayMs() const { return static_cast<int>(m_session->retry().delayMs); }
    [[nodiscard]] bool hasActionRequired() const;
    [[nodiscard]] QVariantList actionToolCalls() const;

    [[nodiscard]] bool hasUsageLimit() const { return m_hasUsageLimit; }
    [[nodiscard]] double usageSpentMicros() const {
        return static_cast<double>(m_usageLimit.spentMicros);
    }
    [[nodiscard]] double usageLimitMicros() const {
        return static_cast<double>(m_usageLimit.limitMicros);
    }
    [[nodiscard]] QDateTime usageResetsAt() const { return m_usageLimit.resetsAt; }

    [[nodiscard]] QStringList prompts() const { return m_prompts; }

    [[nodiscard]] QVariantList diffFiles() const { return m_diffFiles; }
    [[nodiscard]] QString pullRequestUrl() const { return m_pullRequestUrl; }
    [[nodiscard]] bool diffLoading() const { return m_diffLoading; }

    /// Fetches the initial message page, chat metadata, prompt history, and
    /// opens the stream socket.
    void start();

    /// Plan-step parsing (PlanStepParser semantics); list of maps with keys
    /// index, title, body, checked.
    [[nodiscard]] Q_INVOKABLE QVariantList parsePlanSteps(const QString& markdown) const;
    /// Tool-schema parsing (JsonSchemaParser). Returns a map with keys
    /// supported (bool) and fields (list); unsupported schemas yield
    /// supported=false so the caller falls back to a raw JSON editor.
    [[nodiscard]] Q_INVOKABLE QVariantMap parseToolSchema(const QString& schemaJson) const;

public slots:
    void sendMessage(const QString& text, const QString& busyBehavior = QStringLiteral("queue"));
    /// Sends text plus uploaded file attachments with optional per-message
    /// overrides. attachments are maps {fileId, name, mediaType}; options may
    /// carry modelConfigId (string) and planMode (bool).
    void sendMessageWithOptions(const QString& text, const QVariantList& attachments,
                                const QString& busyBehavior, const QVariantMap& options);
    void interrupt();
    void loadOlder();
    void promoteQueued(qint64 queuedId);
    void deleteQueued(qint64 queuedId);
    void submitToolResults(const QJsonArray& results);
    /// Submits one dynamic tool result. outputJson must be a JSON value
    /// (object/array/primitive); invalid JSON is sent as a string.
    void submitToolResult(const QString& toolCallId, const QString& outputJson, bool isError);
    /// Restarts the stream socket, resetting its reconnect budget.
    void reconnect();
    /// Refetches chat metadata (title, archived, plan mode, ...).
    void refreshChat();
    /// Refetches the newest message page, replacing the history (used by the
    /// error callout's Retry to reconcile after failures).
    void refreshMessages();
    void rename(const QString& title);
    void setArchived(bool archived);
    void setPlanModeEnabled(bool enabled);
    void regenerateTitle();
    void fetchPrompts();
    void fetchDiff();
    /// Clears plan mode and posts the canned "Implement the plan above."
    /// message, matching the Android client's implement action.
    void implementPlan();
    void clearUsageLimit();

signals:
    void statusChanged();
    void queueChanged();
    void connectionStateChanged();
    void draftChanged();
    void chatInfoChanged();
    void errorChanged();
    void retryChanged();
    void actionRequiredChanged();
    void promptsChanged();
    void diffChanged();
    void usageLimitChanged();
    void usageLimitExceeded(const ChatUsageLimitExceeded& payload);
    /// A message POST was accepted (queued or started).
    void messageSent();

private:
    void applyChatInfo(const Chat& chat);
    void loadPersistedDraft();
    void persistDraft();

    static constexpr int kMessagePageSize = 50;

    AgentsApiClient* m_api = nullptr;  // non-owning
    QString m_chatId;
    ChatSession* m_session = nullptr;         // Qt parent-owned (this)
    ChatStreamWebSocket* m_stream = nullptr;  // Qt parent-owned (this)
    ChatMessagesModel* m_model = nullptr;     // Qt parent-owned (this)
    QString m_draft;
    bool m_initialLoaded = false;
    Chat m_chat;
    QStringList m_prompts;
    bool m_hasUsageLimit = false;
    ChatUsageLimitExceeded m_usageLimit;
    QVariantList m_diffFiles;
    QString m_pullRequestUrl;
    bool m_diffLoading = false;
};

/// Orchestrator for the Coder Agents feature.
///
/// Owns the global chat watch socket and the chat-list state, probes
/// deployment availability, maintains unread/running/requires-action counts,
/// caches the chat list per deployment, and falls back to REST polling when
/// the watch socket is unhealthy or WebSockets are compiled out.
///
/// Also exposes the create-flow catalog (model configs, MCP servers, the
/// default organization) and small QSettings-persisted UI preferences
/// (last workspace/model selection, composer send shortcut). These live
/// here rather than in SettingsManager because they are per-user UI state,
/// not policy-managed settings.
///
/// Notification dispatch is out of scope here; chatStatusChanged() and
/// actionRequired() give a later stage the hooks it needs.
class AgentsController : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool available READ isAvailable NOTIFY availabilityChanged)
    Q_PROPERTY(QString availabilityReason READ availabilityReason NOTIFY availabilityChanged)
    Q_PROPERTY(bool modelsAvailable READ modelsAvailable NOTIFY availabilityChanged)
    Q_PROPERTY(int unreadCount READ unreadCount NOTIFY countsChanged)
    Q_PROPERTY(int runningCount READ runningCount NOTIFY countsChanged)
    Q_PROPERTY(int requiresActionCount READ requiresActionCount NOTIFY countsChanged)
    Q_PROPERTY(int badgeCount READ badgeCount NOTIFY countsChanged)
    Q_PROPERTY(QString organizationId READ organizationId NOTIFY organizationChanged)
    Q_PROPERTY(QVariantList modelConfigs READ modelConfigs NOTIFY configsChanged)
    Q_PROPERTY(QString defaultModelConfigId READ defaultModelConfigId NOTIFY configsChanged)
    Q_PROPERTY(QVariantList mcpServers READ mcpServers NOTIFY configsChanged)
    Q_PROPERTY(
        QString lastWorkspaceId READ lastWorkspaceId WRITE setLastWorkspaceId NOTIFY uiPrefsChanged)
    Q_PROPERTY(QString lastModelConfigId READ lastModelConfigId WRITE setLastModelConfigId NOTIFY
                   uiPrefsChanged)
    Q_PROPERTY(QString sendShortcut READ sendShortcut WRITE setSendShortcut NOTIFY uiPrefsChanged)

public:
    /// api must outlive this controller.
    explicit AgentsController(AgentsApiClient* api, QObject* parent = nullptr);

    [[nodiscard]] AgentsApiClient* api() const { return m_api; }
    [[nodiscard]] const QList<Chat>& chats() const { return m_chats; }

    [[nodiscard]] bool isAvailable() const { return m_available; }
    [[nodiscard]] QString availabilityReason() const { return m_availabilityReason; }
    [[nodiscard]] bool modelsAvailable() const { return m_modelsAvailable; }
    [[nodiscard]] int unreadCount() const { return m_unreadCount; }
    [[nodiscard]] int runningCount() const { return m_runningCount; }
    [[nodiscard]] int requiresActionCount() const { return m_requiresActionCount; }
    [[nodiscard]] int badgeCount() const {
        return m_unreadCount + m_runningCount + m_requiresActionCount;
    }
    [[nodiscard]] QString organizationId() const { return m_organizationId; }
    [[nodiscard]] QVariantList modelConfigs() const;
    [[nodiscard]] QString defaultModelConfigId() const;
    [[nodiscard]] QVariantList mcpServers() const;

    // Persisted UI preferences (QSettings-backed).
    [[nodiscard]] QString lastWorkspaceId() const { return m_lastWorkspaceId; }
    void setLastWorkspaceId(const QString& id);
    [[nodiscard]] QString lastModelConfigId() const { return m_lastModelConfigId; }
    void setLastModelConfigId(const QString& id);
    /// "enter" (Enter sends, Shift+Enter newline) or "modifier_enter"
    /// (Ctrl+Enter sends, Enter newline).
    [[nodiscard]] QString sendShortcut() const { return m_sendShortcut; }
    void setSendShortcut(const QString& shortcut);

    /// Loads the cached chat list, runs the availability probe, fetches the
    /// live list, and opens the watch socket (or starts polling).
    Q_INVOKABLE void start();
    /// Tears down the watch socket and polling.
    Q_INVOKABLE void stop();
    /// Immediately refetches the chat list.
    Q_INVOKABLE void refreshNow();

    /// Creates and starts a ChatController for the given chat. The returned
    /// object is parented to this controller; deleteLater() closes the chat.
    Q_INVOKABLE ChatController* openChat(const QString& chatId);

    /// Sub-agent children of a chat as maps {id, title, statusString},
    /// derived from the current chat-list state.
    [[nodiscard]] Q_INVOKABLE QVariantList subagentsOf(const QString& chatId) const;

    /// Creates a new chat. attachments are maps {fileId, name, mediaType}
    /// from uploadAttachment(). Emits chatCreated() on success.
    Q_INVOKABLE void createChat(const QString& prompt, const QString& workspaceId,
                                const QString& modelConfigId, const QString& reasoningEffort,
                                const QStringList& mcpServerIds, bool planMode,
                                const QVariantList& attachments);

    // Chat-list row actions (context menu).
    Q_INVOKABLE void pinChat(const QString& chatId, bool pinned);
    Q_INVOKABLE void archiveChat(const QString& chatId, bool archived);
    Q_INVOKABLE void renameChat(const QString& chatId, const QString& title);
    Q_INVOKABLE void regenerateChatTitle(const QString& chatId);

    /// Uploads a local file to the chat file store. Uploads are serialized
    /// so responses correlate to requests; results arrive through
    /// attachmentUploaded()/attachmentUploadFailed() keyed by local path.
    Q_INVOKABLE void uploadAttachment(const QUrl& fileUrl);

signals:
    void availabilityChanged();
    void countsChanged();
    void organizationChanged();
    void configsChanged();
    void uiPrefsChanged();
    /// The full chat list was replaced (initial load, cache load, or poll).
    void chatsReset(const QList<Chat>& chats);
    /// One chat was created or updated by a watch event.
    void chatUpserted(const Chat& chat);
    /// One chat was deleted by a watch event.
    void chatRemoved(const QString& chatId);
    /// A chat created through createChat() is ready to open.
    void chatCreated(const QString& chatId);
    void attachmentUploaded(const QString& localPath, const QString& fileId, const QString& name,
                            const QString& mediaType);
    void attachmentUploadFailed(const QString& localPath, const QString& error);
    /// Hooks for a later notification stage.
    void chatStatusChanged(const Chat& chat, ChatStatus oldStatus, ChatStatus newStatus);
    void actionRequired(const Chat& chat);

private:
    void handleWatchEvent(const ChatWatchEvent& event);
    void setChatList(const QList<Chat>& chats);
    void applyUpsert(const Chat& chat);
    void recomputeCounts();
    void updateAvailability();
    void updatePollingState();
    void loadCache();
    void saveCache(const QJsonArray& raw);
    void loadUiPrefs();
    void startNextUpload();
    [[nodiscard]] QString cacheDirForDeployment() const;
    [[nodiscard]] ChatStatus knownStatusOf(const QString& chatId) const;

    static constexpr int kPollIntervalMs = 15000;

    AgentsApiClient* m_api = nullptr;       // non-owning
    ChatWatchWebSocket* m_watch = nullptr;  // Qt parent-owned (this)
    QTimer m_pollTimer;
    QList<Chat> m_chats;
    bool m_started = false;

    // Availability probe state.
    bool m_canCreate = false;
    bool m_endpointOk = false;
    bool m_daemonMissing = false;
    bool m_modelsAvailable = false;
    bool m_available = false;
    QString m_availabilityReason;

    int m_unreadCount = 0;
    int m_runningCount = 0;
    int m_requiresActionCount = 0;

    QString m_organizationId;
    QList<ChatModelConfig> m_modelConfigs;
    QList<McpServerConfig> m_mcpServers;

    QString m_lastWorkspaceId;
    QString m_lastModelConfigId;
    QString m_sendShortcut = QStringLiteral("enter");

    // Serialized attachment upload queue: {localPath, name, mediaType}.
    struct PendingUpload {
        QString localPath;
        QString name;
        QString mediaType;
    };
    QQueue<PendingUpload> m_uploadQueue;
    bool m_uploadInFlight = false;
};

#endif  // AGENTSCONTROLLER_H
