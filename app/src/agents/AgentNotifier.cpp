#include "agents/AgentNotifier.h"

#include "agents/AgentsController.h"
#include "notifications/NotificationManager.h"
#include "settings/SettingsManager.h"

namespace {

/// Dedupe-set size cap; the set is cleared wholesale when reached (keys are
/// only ever compared for exact matches, so losing history is harmless: the
/// worst case is one repeated notification).
constexpr int kMaxSeenKeys = 1024;

}  // namespace

AgentNotifier::AgentNotifier(AgentsController* controller, NotificationManager* notifications,
                             SettingsManager* settings, QObject* parent)
    : QObject(parent),
      m_controller(controller),
      m_notifications(notifications),
      m_settings(settings) {
    connect(m_controller, &AgentsController::chatStatusChanged, this,
            &AgentNotifier::onStatusChanged);
    connect(m_controller, &AgentsController::actionRequired, this,
            &AgentNotifier::onActionRequired);
    // Chat ids are deployment-scoped UUIDs; the dedupe history is useless
    // after a deployment switch or logout, so drop it.
    connect(m_controller, &AgentsController::stopped, this, [this]() { m_seen.clear(); });
}

void AgentNotifier::onStatusChanged(const Chat& chat, ChatStatus /*oldStatus*/,
                                    ChatStatus newStatus) {
    if (newStatus == ChatStatus::Completed) {
        maybeNotify(chat, QStringLiteral("completed"), QStringLiteral("Finished"));
    } else if (newStatus == ChatStatus::Error) {
        const QString msg = chat.lastError.message.isEmpty() ? QStringLiteral("Unknown error")
                                                             : chat.lastError.message;
        maybeNotify(chat, QStringLiteral("error"), QStringLiteral("Failed: %1").arg(msg));
    }
}

void AgentNotifier::onActionRequired(const Chat& chat) {
    maybeNotify(chat, QStringLiteral("requires_action"), QStringLiteral("Needs input"));
}

void AgentNotifier::maybeNotify(const Chat& chat, const QString& statusKey, const QString& body) {
    // NotificationManager already enforces the global notificationsEnabled
    // setting; this adds the agent-specific toggle on top.
    if (!m_settings->agentNotificationsEnabled()) return;
    // The chat currently open in an active window never notifies.
    if (!chat.id.isEmpty() && chat.id == m_controller->focusedChatId()) return;

    const QString key = chat.id + QLatin1Char('|') + statusKey + QLatin1Char('|') +
                        chat.updatedAt.toString(Qt::ISODateWithMs);
    if (m_seen.contains(key)) return;
    if (m_seen.size() >= kMaxSeenKeys) m_seen.clear();
    m_seen.insert(key);

    m_notifications->notify(composeTitle(chat), body, QStringLiteral("AgentUpdate"));
}

QString AgentNotifier::composeTitle(const Chat& chat) const {
    const QString own = chat.title.isEmpty() ? QStringLiteral("Untitled conversation") : chat.title;
    if (chat.parentChatId.isEmpty()) return own;
    for (const Chat& root : m_controller->chats()) {
        if (root.id != chat.parentChatId) continue;
        if (!root.title.isEmpty() && root.title != own)
            return root.title + QStringLiteral(" · ") + own;
        break;
    }
    return own;
}
