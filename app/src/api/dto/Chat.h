#ifndef CODER_DTO_CHAT_H
#define CODER_DTO_CHAT_H

#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QMetaType>
#include <QString>
#include <QVariantMap>

// DTOs for the experimental Coder Agents chat API.
// JSON field names match coder/coder codersdk/chats.go exactly.

// ---------------------------------------------------------------------------
// ChatStatus
// ---------------------------------------------------------------------------

enum class ChatStatus {
    Waiting,
    Pending,
    Running,
    Paused,
    Completed,
    Error,
    RequiresAction,
    Interrupting,
    Unknown,
};

[[nodiscard]] inline ChatStatus chatStatusFromString(const QString& s) {
    if (s == QLatin1String("waiting")) return ChatStatus::Waiting;
    if (s == QLatin1String("pending")) return ChatStatus::Pending;
    if (s == QLatin1String("running")) return ChatStatus::Running;
    if (s == QLatin1String("paused")) return ChatStatus::Paused;
    if (s == QLatin1String("completed")) return ChatStatus::Completed;
    if (s == QLatin1String("error")) return ChatStatus::Error;
    if (s == QLatin1String("requires_action")) return ChatStatus::RequiresAction;
    if (s == QLatin1String("interrupting")) return ChatStatus::Interrupting;
    return ChatStatus::Unknown;
}

[[nodiscard]] inline QString chatStatusToString(ChatStatus status) {
    switch (status) {
        case ChatStatus::Waiting:
            return QStringLiteral("waiting");
        case ChatStatus::Pending:
            return QStringLiteral("pending");
        case ChatStatus::Running:
            return QStringLiteral("running");
        case ChatStatus::Paused:
            return QStringLiteral("paused");
        case ChatStatus::Completed:
            return QStringLiteral("completed");
        case ChatStatus::Error:
            return QStringLiteral("error");
        case ChatStatus::RequiresAction:
            return QStringLiteral("requires_action");
        case ChatStatus::Interrupting:
            return QStringLiteral("interrupting");
        case ChatStatus::Unknown:
            break;
    }
    return QStringLiteral("unknown");
}

Q_DECLARE_METATYPE(ChatStatus)

// ---------------------------------------------------------------------------
// ChatError (codersdk.ChatError) - persisted last_error and live stream errors
// ---------------------------------------------------------------------------

struct ChatError {
    QString message;
    QString detail;
    QString kind;  // generic|overloaded|rate_limit|timeout|auth|config|usage_limit|...
    QString provider;
    bool retryable = false;
    int statusCode = 0;

    [[nodiscard]] bool isEmpty() const { return message.isEmpty() && kind.isEmpty(); }

    static ChatError fromJson(const QJsonObject& obj) {
        ChatError e;
        e.message = obj.value(QLatin1String("message")).toString();
        e.detail = obj.value(QLatin1String("detail")).toString();
        e.kind = obj.value(QLatin1String("kind")).toString();
        e.provider = obj.value(QLatin1String("provider")).toString();
        e.retryable = obj.value(QLatin1String("retryable")).toBool();
        e.statusCode = obj.value(QLatin1String("status_code")).toInt();
        return e;
    }
};

Q_DECLARE_METATYPE(ChatError)

// ---------------------------------------------------------------------------
// ChatDiffStatusInfo (codersdk.ChatDiffStatus, subset used by the UI)
// ---------------------------------------------------------------------------

struct ChatDiffStatusInfo {
    bool hasValue = false;
    QString url;
    QString pullRequestState;
    QString pullRequestTitle;
    bool pullRequestDraft = false;
    bool changesRequested = false;
    int additions = 0;
    int deletions = 0;
    int changedFiles = 0;
    int prNumber = 0;

    static ChatDiffStatusInfo fromJson(const QJsonObject& obj) {
        ChatDiffStatusInfo d;
        if (obj.isEmpty()) return d;
        d.hasValue = true;
        d.url = obj.value(QLatin1String("url")).toString();
        d.pullRequestState = obj.value(QLatin1String("pull_request_state")).toString();
        d.pullRequestTitle = obj.value(QLatin1String("pull_request_title")).toString();
        d.pullRequestDraft = obj.value(QLatin1String("pull_request_draft")).toBool();
        d.changesRequested = obj.value(QLatin1String("changes_requested")).toBool();
        d.additions = obj.value(QLatin1String("additions")).toInt();
        d.deletions = obj.value(QLatin1String("deletions")).toInt();
        d.changedFiles = obj.value(QLatin1String("changed_files")).toInt();
        d.prNumber = obj.value(QLatin1String("pr_number")).toInt();
        return d;
    }
};

Q_DECLARE_METATYPE(ChatDiffStatusInfo)

// ---------------------------------------------------------------------------
// Chat (codersdk.Chat)
// ---------------------------------------------------------------------------

struct Chat {
    QString id;
    QString organizationId;
    QString ownerId;
    QString ownerUsername;
    QString workspaceId;
    QString parentChatId;
    QString rootChatId;
    QString lastModelConfigId;
    QString title;
    ChatStatus status = ChatStatus::Unknown;
    QString statusString;
    QString planMode;  // "plan" when plan mode is active, empty otherwise
    ChatError lastError;
    bool hasLastError = false;
    ChatDiffStatusInfo diffStatus;
    QDateTime createdAt;
    QDateTime updatedAt;
    bool archived = false;
    int pinOrder = 0;  // > 0 means pinned; ascending pin position
    QVariantMap labels;
    bool hasUnread = false;
    QList<Chat> children;  // sub-agent chats, nesting depth is capped at 1

    [[nodiscard]] bool isPinned() const { return pinOrder > 0; }
    [[nodiscard]] bool isSubagent() const { return !parentChatId.isEmpty(); }

    static Chat fromJson(const QJsonObject& obj) {
        Chat c;
        c.id = obj.value(QLatin1String("id")).toString();
        c.organizationId = obj.value(QLatin1String("organization_id")).toString();
        c.ownerId = obj.value(QLatin1String("owner_id")).toString();
        c.ownerUsername = obj.value(QLatin1String("owner_username")).toString();
        c.workspaceId = obj.value(QLatin1String("workspace_id")).toString();
        c.parentChatId = obj.value(QLatin1String("parent_chat_id")).toString();
        c.rootChatId = obj.value(QLatin1String("root_chat_id")).toString();
        c.lastModelConfigId = obj.value(QLatin1String("last_model_config_id")).toString();
        c.title = obj.value(QLatin1String("title")).toString();
        c.statusString = obj.value(QLatin1String("status")).toString();
        c.status = chatStatusFromString(c.statusString);
        c.planMode = obj.value(QLatin1String("plan_mode")).toString();
        if (obj.value(QLatin1String("last_error")).isObject()) {
            c.lastError = ChatError::fromJson(obj.value(QLatin1String("last_error")).toObject());
            c.hasLastError = true;
        }
        c.diffStatus =
            ChatDiffStatusInfo::fromJson(obj.value(QLatin1String("diff_status")).toObject());
        c.createdAt = QDateTime::fromString(obj.value(QLatin1String("created_at")).toString(),
                                            Qt::ISODateWithMs);
        c.updatedAt = QDateTime::fromString(obj.value(QLatin1String("updated_at")).toString(),
                                            Qt::ISODateWithMs);
        c.archived = obj.value(QLatin1String("archived")).toBool();
        c.pinOrder = obj.value(QLatin1String("pin_order")).toInt();
        c.labels = obj.value(QLatin1String("labels")).toObject().toVariantMap();
        c.hasUnread = obj.value(QLatin1String("has_unread")).toBool();
        const QJsonArray childrenArr = obj.value(QLatin1String("children")).toArray();
        c.children.reserve(childrenArr.size());
        for (const QJsonValue& v : childrenArr) c.children.append(fromJson(v.toObject()));
        return c;
    }

    static QList<Chat> listFromJson(const QJsonArray& arr) {
        QList<Chat> list;
        list.reserve(arr.size());
        for (const QJsonValue& v : arr) list.append(fromJson(v.toObject()));
        return list;
    }
};

Q_DECLARE_METATYPE(Chat)

#endif  // CODER_DTO_CHAT_H
