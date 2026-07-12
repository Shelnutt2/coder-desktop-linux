#ifndef AGENTSCONTROLLER_H
#define AGENTSCONTROLLER_H

#include <QHash>
#include <QJsonArray>
#include <QList>
#include <QObject>
#include <QString>
#include <QTimer>

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
    Q_PROPERTY(int connectionState READ connectionState NOTIFY connectionStateChanged)
    Q_PROPERTY(QString draft READ draft WRITE setDraft NOTIFY draftChanged)

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
    [[nodiscard]] int connectionState() const {
        return static_cast<int>(m_stream->connectionState());
    }
    [[nodiscard]] QString draft() const { return m_draft; }
    void setDraft(const QString& draft);

    /// Fetches the initial message page and opens the stream socket.
    void start();

public slots:
    void sendMessage(const QString& text, const QString& busyBehavior = QStringLiteral("queue"));
    void interrupt();
    void loadOlder();
    void promoteQueued(qint64 queuedId);
    void deleteQueued(qint64 queuedId);
    void submitToolResults(const QJsonArray& results);

signals:
    void statusChanged();
    void queueChanged();
    void connectionStateChanged();
    void draftChanged();
    void usageLimitExceeded(const ChatUsageLimitExceeded& payload);

private:
    static constexpr int kMessagePageSize = 50;

    AgentsApiClient* m_api = nullptr;  // non-owning
    QString m_chatId;
    ChatSession* m_session = nullptr;         // Qt parent-owned (this)
    ChatStreamWebSocket* m_stream = nullptr;  // Qt parent-owned (this)
    ChatMessagesModel* m_model = nullptr;     // Qt parent-owned (this)
    QString m_draft;
    bool m_initialLoaded = false;
};

/// Orchestrator for the Coder Agents feature.
///
/// Owns the global chat watch socket and the chat-list state, probes
/// deployment availability, maintains unread/running/requires-action counts,
/// caches the chat list per deployment, and falls back to REST polling when
/// the watch socket is unhealthy or WebSockets are compiled out.
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

signals:
    void availabilityChanged();
    void countsChanged();
    /// The full chat list was replaced (initial load, cache load, or poll).
    void chatsReset(const QList<Chat>& chats);
    /// One chat was created or updated by a watch event.
    void chatUpserted(const Chat& chat);
    /// One chat was deleted by a watch event.
    void chatRemoved(const QString& chatId);
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
};

#endif  // AGENTSCONTROLLER_H
