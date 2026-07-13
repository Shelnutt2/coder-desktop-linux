#ifndef AGENTNOTIFIER_H
#define AGENTNOTIFIER_H

#include <QObject>
#include <QSet>
#include <QString>

#include "api/dto/Chat.h"

class AgentsController;
class NotificationManager;
class SettingsManager;

/// Routes AgentsController status/action events to desktop notifications.
///
/// Mirrors the Android WatchEventDispatcher semantics: notify only on
/// terminal status transitions (completed/error) and on action_required,
/// skip the chat currently focused in the UI, and honor the
/// agentNotificationsEnabled setting. An in-memory dedupe set keyed by
/// chatId+status+updatedAt prevents the watch socket and the polling
/// fallback from double-notifying the same transition.
class AgentNotifier : public QObject {
    Q_OBJECT

public:
    /// All pointers are non-owning and must outlive this notifier.
    explicit AgentNotifier(AgentsController* controller, NotificationManager* notifications,
                           SettingsManager* settings, QObject* parent = nullptr);

private:
    void onStatusChanged(const Chat& chat, ChatStatus oldStatus, ChatStatus newStatus);
    void onActionRequired(const Chat& chat);
    void maybeNotify(const Chat& chat, const QString& statusKey, const QString& body);
    /// "<root title> · <subagent title>" for depth-1 subagent chats,
    /// otherwise the chat's own title (or "Untitled conversation").
    [[nodiscard]] QString composeTitle(const Chat& chat) const;

    AgentsController* m_controller = nullptr;        // non-owning
    NotificationManager* m_notifications = nullptr;  // non-owning
    SettingsManager* m_settings = nullptr;           // non-owning
    QSet<QString> m_seen;
};

#endif  // AGENTNOTIFIER_H
