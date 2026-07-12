#ifndef CODER_DTO_CHATSTREAMEVENT_H
#define CODER_DTO_CHATSTREAMEVENT_H

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QList>
#include <QMetaType>
#include <QString>

#include "api/dto/Chat.h"
#include "api/dto/ChatMessage.h"

// Stream and watch event DTOs for the experimental Coder Agents chat API.
// JSON field names match coder/coder codersdk/chats.go exactly.

// ---------------------------------------------------------------------------
// ChatStreamEventType
// ---------------------------------------------------------------------------

enum class ChatStreamEventType {
    MessagePart,
    Message,
    Status,
    Error,
    QueueUpdate,
    Retry,
    ActionRequired,
    PreviewReset,
    HistoryReset,
    Unknown,
};

[[nodiscard]] inline ChatStreamEventType chatStreamEventTypeFromString(const QString& s) {
    if (s == QLatin1String("message_part")) return ChatStreamEventType::MessagePart;
    if (s == QLatin1String("message")) return ChatStreamEventType::Message;
    if (s == QLatin1String("status")) return ChatStreamEventType::Status;
    if (s == QLatin1String("error")) return ChatStreamEventType::Error;
    if (s == QLatin1String("queue_update")) return ChatStreamEventType::QueueUpdate;
    if (s == QLatin1String("retry")) return ChatStreamEventType::Retry;
    if (s == QLatin1String("action_required")) return ChatStreamEventType::ActionRequired;
    if (s == QLatin1String("preview_reset")) return ChatStreamEventType::PreviewReset;
    if (s == QLatin1String("history_reset")) return ChatStreamEventType::HistoryReset;
    return ChatStreamEventType::Unknown;
}

// ---------------------------------------------------------------------------
// Payloads
// ---------------------------------------------------------------------------

/// codersdk.ChatStreamMessagePart - a streamed message part delta.
struct ChatStreamMessagePart {
    QString role;
    ChatMessagePart part;
    qint64 seq = 0;
    qint64 historyVersion = 0;
    qint64 generationAttempt = 0;

    static ChatStreamMessagePart fromJson(const QJsonObject& obj) {
        ChatStreamMessagePart p;
        p.role = obj.value(QLatin1String("role")).toString();
        p.part = ChatMessagePart::fromJson(obj.value(QLatin1String("part")).toObject());
        p.seq = obj.value(QLatin1String("seq")).toVariant().toLongLong();
        p.historyVersion = obj.value(QLatin1String("history_version")).toVariant().toLongLong();
        p.generationAttempt =
            obj.value(QLatin1String("generation_attempt")).toVariant().toLongLong();
        return p;
    }
};

Q_DECLARE_METATYPE(ChatStreamMessagePart)

/// codersdk.ChatStreamRetry - server auto-retry of a failed LLM call.
struct ChatStreamRetry {
    int attempt = 0;
    qint64 delayMs = 0;
    QString error;
    QString kind;
    QDateTime retryingAt;

    static ChatStreamRetry fromJson(const QJsonObject& obj) {
        ChatStreamRetry r;
        r.attempt = obj.value(QLatin1String("attempt")).toInt();
        r.delayMs = obj.value(QLatin1String("delay_ms")).toVariant().toLongLong();
        r.error = obj.value(QLatin1String("error")).toString();
        r.kind = obj.value(QLatin1String("kind")).toString();
        r.retryingAt = QDateTime::fromString(obj.value(QLatin1String("retrying_at")).toString(),
                                             Qt::ISODateWithMs);
        return r;
    }
};

Q_DECLARE_METATYPE(ChatStreamRetry)

/// codersdk.ChatStreamToolCall - a pending dynamic tool call.
struct ChatStreamToolCall {
    QString toolCallId;
    QString toolName;
    QString args;  // raw JSON text of the tool arguments

    static ChatStreamToolCall fromJson(const QJsonObject& obj) {
        ChatStreamToolCall t;
        t.toolCallId = obj.value(QLatin1String("tool_call_id")).toString();
        t.toolName = obj.value(QLatin1String("tool_name")).toString();
        t.args = obj.value(QLatin1String("args")).toString();
        return t;
    }
};

Q_DECLARE_METATYPE(ChatStreamToolCall)

/// codersdk.ChatStreamActionRequired - tool calls the client must execute.
struct ChatStreamActionRequired {
    QList<ChatStreamToolCall> toolCalls;

    static ChatStreamActionRequired fromJson(const QJsonObject& obj) {
        ChatStreamActionRequired a;
        const QJsonArray arr = obj.value(QLatin1String("tool_calls")).toArray();
        a.toolCalls.reserve(arr.size());
        for (const QJsonValue& v : arr)
            a.toolCalls.append(ChatStreamToolCall::fromJson(v.toObject()));
        return a;
    }
};

Q_DECLARE_METATYPE(ChatStreamActionRequired)

// ---------------------------------------------------------------------------
// ChatStreamEvent (codersdk.ChatStreamEvent)
// ---------------------------------------------------------------------------

struct ChatStreamEvent {
    ChatStreamEventType type = ChatStreamEventType::Unknown;
    QString typeString;
    QString chatId;

    ChatMessage message;
    bool hasMessage = false;

    ChatStreamMessagePart messagePart;
    bool hasMessagePart = false;

    ChatStatus status = ChatStatus::Unknown;
    QString statusString;
    bool hasStatus = false;

    ChatError error;
    bool hasError = false;

    ChatStreamRetry retry;
    bool hasRetry = false;

    QList<ChatQueuedMessage> queuedMessages;

    ChatStreamActionRequired actionRequired;
    bool hasActionRequired = false;

    static ChatStreamEvent fromJson(const QJsonObject& obj) {
        ChatStreamEvent e;
        e.typeString = obj.value(QLatin1String("type")).toString();
        e.type = chatStreamEventTypeFromString(e.typeString);
        e.chatId = obj.value(QLatin1String("chat_id")).toString();
        if (obj.value(QLatin1String("message")).isObject()) {
            e.message = ChatMessage::fromJson(obj.value(QLatin1String("message")).toObject());
            e.hasMessage = true;
        }
        if (obj.value(QLatin1String("message_part")).isObject()) {
            e.messagePart = ChatStreamMessagePart::fromJson(
                obj.value(QLatin1String("message_part")).toObject());
            e.hasMessagePart = true;
        }
        if (obj.value(QLatin1String("status")).isObject()) {
            const QJsonObject statusObj = obj.value(QLatin1String("status")).toObject();
            e.statusString = statusObj.value(QLatin1String("status")).toString();
            e.status = chatStatusFromString(e.statusString);
            e.hasStatus = true;
        }
        if (obj.value(QLatin1String("error")).isObject()) {
            e.error = ChatError::fromJson(obj.value(QLatin1String("error")).toObject());
            e.hasError = true;
        }
        if (obj.value(QLatin1String("retry")).isObject()) {
            e.retry = ChatStreamRetry::fromJson(obj.value(QLatin1String("retry")).toObject());
            e.hasRetry = true;
        }
        e.queuedMessages =
            ChatQueuedMessage::listFromJson(obj.value(QLatin1String("queued_messages")).toArray());
        if (obj.value(QLatin1String("action_required")).isObject()) {
            e.actionRequired = ChatStreamActionRequired::fromJson(
                obj.value(QLatin1String("action_required")).toObject());
            e.hasActionRequired = true;
        }
        return e;
    }
};

Q_DECLARE_METATYPE(ChatStreamEvent)

// ---------------------------------------------------------------------------
// ChatWatchEvent (codersdk.ChatWatchEvent) - global chat watch stream
// ---------------------------------------------------------------------------

struct ChatWatchEvent {
    QString kind;  // created|status_change|summary_change|title_change|deleted|...
    Chat chat;
    QList<ChatStreamToolCall> toolCalls;  // populated when kind is action_required

    static ChatWatchEvent fromJson(const QJsonObject& obj) {
        ChatWatchEvent e;
        e.kind = obj.value(QLatin1String("kind")).toString();
        e.chat = Chat::fromJson(obj.value(QLatin1String("chat")).toObject());
        const QJsonArray arr = obj.value(QLatin1String("tool_calls")).toArray();
        e.toolCalls.reserve(arr.size());
        for (const QJsonValue& v : arr)
            e.toolCalls.append(ChatStreamToolCall::fromJson(v.toObject()));
        return e;
    }
};

Q_DECLARE_METATYPE(ChatWatchEvent)

// ---------------------------------------------------------------------------
// Frame parsing
// ---------------------------------------------------------------------------

/// Parses one chat stream WebSocket text frame. The server sends JSON arrays
/// of events; a single event object is also accepted. An empty array is a
/// heartbeat and yields an empty list. When ok is non-null it is set to
/// whether the frame was valid JSON of an accepted shape.
[[nodiscard]] inline QList<ChatStreamEvent> parseChatStreamFrame(const QString& frame,
                                                                 bool* ok = nullptr) {
    QList<ChatStreamEvent> events;
    QJsonParseError parseError{};
    const QJsonDocument doc = QJsonDocument::fromJson(frame.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        if (ok) *ok = false;
        return events;
    }
    if (doc.isArray()) {
        const QJsonArray arr = doc.array();
        events.reserve(arr.size());
        for (const QJsonValue& v : arr) events.append(ChatStreamEvent::fromJson(v.toObject()));
        if (ok) *ok = true;
        return events;
    }
    if (doc.isObject()) {
        events.append(ChatStreamEvent::fromJson(doc.object()));
        if (ok) *ok = true;
        return events;
    }
    if (ok) *ok = false;
    return events;
}

#endif  // CODER_DTO_CHATSTREAMEVENT_H
