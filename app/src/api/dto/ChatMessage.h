#ifndef CODER_DTO_CHATMESSAGE_H
#define CODER_DTO_CHATMESSAGE_H

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QList>
#include <QMetaType>
#include <QString>

// Message DTOs for the experimental Coder Agents chat API.
// JSON field names match coder/coder codersdk/chats.go exactly.

// ---------------------------------------------------------------------------
// ChatMessagePartType
// ---------------------------------------------------------------------------

enum class ChatMessagePartType {
    Text,
    Reasoning,
    ToolCall,
    ToolResult,
    Source,
    File,
    FileReference,
    ContextFile,
    Skill,
    Image,
    Unknown,
};

[[nodiscard]] inline ChatMessagePartType chatMessagePartTypeFromString(const QString& s) {
    if (s == QLatin1String("text")) return ChatMessagePartType::Text;
    if (s == QLatin1String("reasoning")) return ChatMessagePartType::Reasoning;
    if (s == QLatin1String("tool-call")) return ChatMessagePartType::ToolCall;
    if (s == QLatin1String("tool-result")) return ChatMessagePartType::ToolResult;
    if (s == QLatin1String("source")) return ChatMessagePartType::Source;
    if (s == QLatin1String("file")) return ChatMessagePartType::File;
    if (s == QLatin1String("file-reference")) return ChatMessagePartType::FileReference;
    if (s == QLatin1String("context-file")) return ChatMessagePartType::ContextFile;
    if (s == QLatin1String("skill")) return ChatMessagePartType::Skill;
    if (s == QLatin1String("image")) return ChatMessagePartType::Image;
    return ChatMessagePartType::Unknown;
}

// ---------------------------------------------------------------------------
// ChatMessagePart (codersdk.ChatMessagePart) - tagged union on "type".
//
// Unknown part types keep the raw JSON object so they round-trip losslessly
// through toJson(), preserving forward compatibility with new server types.
// ---------------------------------------------------------------------------

struct ChatMessagePart {
    ChatMessagePartType type = ChatMessagePartType::Unknown;
    QString typeString;

    // text / reasoning
    QString text;
    QString signature;

    // tool-call / tool-result (merged by toolCallId during streaming)
    QString toolCallId;
    QString toolName;
    QJsonObject args;      // tool-call arguments; empty object means "no update"
    QString argsDelta;     // streaming-only incremental args JSON text
    QJsonValue result;     // tool-result payload (arbitrary JSON)
    QString resultDelta;   // streaming-only incremental result text
    bool isError = false;  // tool-result error flag
    bool providerExecuted = false;

    // source
    QString sourceId;
    QString url;
    QString title;

    // file / image
    QString mediaType;
    QString name;
    QString fileId;

    // file-reference
    QString fileName;
    int startLine = 0;
    int endLine = 0;
    QString content;

    // context-file
    QString contextFilePath;
    bool contextFileTruncated = false;

    // skill
    QString skillName;
    QString skillDescription;

    // Timestamps carried by tool-call, tool-result, and reasoning parts.
    QDateTime createdAt;
    QDateTime completedAt;

    // Raw wire object; authoritative for Unknown parts.
    QJsonObject raw;

    static ChatMessagePart fromJson(const QJsonObject& obj) {
        ChatMessagePart p;
        p.raw = obj;
        p.typeString = obj.value(QLatin1String("type")).toString();
        p.type = chatMessagePartTypeFromString(p.typeString);
        p.text = obj.value(QLatin1String("text")).toString();
        p.signature = obj.value(QLatin1String("signature")).toString();
        p.toolCallId = obj.value(QLatin1String("tool_call_id")).toString();
        p.toolName = obj.value(QLatin1String("tool_name")).toString();
        p.args = obj.value(QLatin1String("args")).toObject();
        p.argsDelta = obj.value(QLatin1String("args_delta")).toString();
        if (obj.contains(QLatin1String("result"))) {
            p.result = obj.value(QLatin1String("result"));
        }
        p.resultDelta = obj.value(QLatin1String("result_delta")).toString();
        p.isError = obj.value(QLatin1String("is_error")).toBool();
        p.providerExecuted = obj.value(QLatin1String("provider_executed")).toBool();
        p.sourceId = obj.value(QLatin1String("source_id")).toString();
        p.url = obj.value(QLatin1String("url")).toString();
        p.title = obj.value(QLatin1String("title")).toString();
        p.mediaType = obj.value(QLatin1String("media_type")).toString();
        p.name = obj.value(QLatin1String("name")).toString();
        p.fileId = obj.value(QLatin1String("file_id")).toString();
        p.fileName = obj.value(QLatin1String("file_name")).toString();
        p.startLine = obj.value(QLatin1String("start_line")).toInt();
        p.endLine = obj.value(QLatin1String("end_line")).toInt();
        p.content = obj.value(QLatin1String("content")).toString();
        p.contextFilePath = obj.value(QLatin1String("context_file_path")).toString();
        p.contextFileTruncated = obj.value(QLatin1String("context_file_truncated")).toBool();
        p.skillName = obj.value(QLatin1String("skill_name")).toString();
        p.skillDescription = obj.value(QLatin1String("skill_description")).toString();
        p.createdAt = QDateTime::fromString(obj.value(QLatin1String("created_at")).toString(),
                                            Qt::ISODateWithMs);
        p.completedAt = QDateTime::fromString(obj.value(QLatin1String("completed_at")).toString(),
                                              Qt::ISODateWithMs);
        return p;
    }

    [[nodiscard]] QJsonObject toJson() const {
        // Unknown parts round-trip through the raw wire object.
        if (type == ChatMessagePartType::Unknown) return raw;

        QJsonObject obj;
        obj.insert(QLatin1String("type"), typeString);
        switch (type) {
            case ChatMessagePartType::Text:
            case ChatMessagePartType::Reasoning:
                obj.insert(QLatin1String("text"), text);
                if (!signature.isEmpty()) obj.insert(QLatin1String("signature"), signature);
                break;
            case ChatMessagePartType::ToolCall:
                if (!toolCallId.isEmpty()) obj.insert(QLatin1String("tool_call_id"), toolCallId);
                if (!toolName.isEmpty()) obj.insert(QLatin1String("tool_name"), toolName);
                if (!args.isEmpty()) obj.insert(QLatin1String("args"), args);
                if (!argsDelta.isEmpty()) obj.insert(QLatin1String("args_delta"), argsDelta);
                if (providerExecuted) obj.insert(QLatin1String("provider_executed"), true);
                break;
            case ChatMessagePartType::ToolResult:
                if (!toolCallId.isEmpty()) obj.insert(QLatin1String("tool_call_id"), toolCallId);
                if (!toolName.isEmpty()) obj.insert(QLatin1String("tool_name"), toolName);
                if (!result.isUndefined()) obj.insert(QLatin1String("result"), result);
                if (!resultDelta.isEmpty()) obj.insert(QLatin1String("result_delta"), resultDelta);
                if (isError) obj.insert(QLatin1String("is_error"), true);
                if (providerExecuted) obj.insert(QLatin1String("provider_executed"), true);
                break;
            case ChatMessagePartType::Source:
                if (!sourceId.isEmpty()) obj.insert(QLatin1String("source_id"), sourceId);
                obj.insert(QLatin1String("url"), url);
                if (!title.isEmpty()) obj.insert(QLatin1String("title"), title);
                break;
            case ChatMessagePartType::File:
            case ChatMessagePartType::Image:
                obj.insert(QLatin1String("media_type"), mediaType);
                if (!name.isEmpty()) obj.insert(QLatin1String("name"), name);
                if (!fileId.isEmpty()) obj.insert(QLatin1String("file_id"), fileId);
                break;
            case ChatMessagePartType::FileReference:
                obj.insert(QLatin1String("file_name"), fileName);
                obj.insert(QLatin1String("start_line"), startLine);
                obj.insert(QLatin1String("end_line"), endLine);
                obj.insert(QLatin1String("content"), content);
                break;
            case ChatMessagePartType::ContextFile:
                obj.insert(QLatin1String("context_file_path"), contextFilePath);
                if (contextFileTruncated) obj.insert(QLatin1String("context_file_truncated"), true);
                break;
            case ChatMessagePartType::Skill:
                obj.insert(QLatin1String("skill_name"), skillName);
                if (!skillDescription.isEmpty())
                    obj.insert(QLatin1String("skill_description"), skillDescription);
                break;
            case ChatMessagePartType::Unknown:
                break;
        }
        if (createdAt.isValid())
            obj.insert(QLatin1String("created_at"), createdAt.toString(Qt::ISODateWithMs));
        if (completedAt.isValid())
            obj.insert(QLatin1String("completed_at"), completedAt.toString(Qt::ISODateWithMs));
        return obj;
    }

    static QList<ChatMessagePart> listFromJson(const QJsonArray& arr) {
        QList<ChatMessagePart> list;
        list.reserve(arr.size());
        for (const QJsonValue& v : arr) list.append(fromJson(v.toObject()));
        return list;
    }
};

Q_DECLARE_METATYPE(ChatMessagePart)

// ---------------------------------------------------------------------------
// ChatMessageUsage (codersdk.ChatMessageUsage)
// ---------------------------------------------------------------------------

struct ChatMessageUsage {
    bool hasValue = false;
    qint64 totalTokens = -1;   // -1 when absent
    qint64 contextLimit = -1;  // -1 when absent

    static ChatMessageUsage fromJson(const QJsonObject& obj) {
        ChatMessageUsage u;
        if (obj.isEmpty()) return u;
        u.hasValue = true;
        if (obj.contains(QLatin1String("total_tokens")))
            u.totalTokens = obj.value(QLatin1String("total_tokens")).toVariant().toLongLong();
        if (obj.contains(QLatin1String("context_limit")))
            u.contextLimit = obj.value(QLatin1String("context_limit")).toVariant().toLongLong();
        return u;
    }
};

Q_DECLARE_METATYPE(ChatMessageUsage)

// ---------------------------------------------------------------------------
// ChatMessage (codersdk.ChatMessage) - message ids are int64
// ---------------------------------------------------------------------------

struct ChatMessage {
    qint64 id = 0;
    QString chatId;
    QString role;  // system|user|assistant|tool
    QDateTime createdAt;
    QList<ChatMessagePart> parts;
    ChatMessageUsage usage;

    static ChatMessage fromJson(const QJsonObject& obj) {
        ChatMessage m;
        m.id = obj.value(QLatin1String("id")).toVariant().toLongLong();
        m.chatId = obj.value(QLatin1String("chat_id")).toString();
        m.role = obj.value(QLatin1String("role")).toString();
        m.createdAt = QDateTime::fromString(obj.value(QLatin1String("created_at")).toString(),
                                            Qt::ISODateWithMs);
        m.parts = ChatMessagePart::listFromJson(obj.value(QLatin1String("content")).toArray());
        m.usage = ChatMessageUsage::fromJson(obj.value(QLatin1String("usage")).toObject());
        return m;
    }

    static QList<ChatMessage> listFromJson(const QJsonArray& arr) {
        QList<ChatMessage> list;
        list.reserve(arr.size());
        for (const QJsonValue& v : arr) list.append(fromJson(v.toObject()));
        return list;
    }
};

Q_DECLARE_METATYPE(ChatMessage)

// ---------------------------------------------------------------------------
// ChatQueuedMessage (codersdk.ChatQueuedMessage)
// ---------------------------------------------------------------------------

struct ChatQueuedMessage {
    qint64 id = 0;
    QString chatId;
    QList<ChatMessagePart> content;
    QDateTime createdAt;

    static ChatQueuedMessage fromJson(const QJsonObject& obj) {
        ChatQueuedMessage q;
        q.id = obj.value(QLatin1String("id")).toVariant().toLongLong();
        q.chatId = obj.value(QLatin1String("chat_id")).toString();
        q.content = ChatMessagePart::listFromJson(obj.value(QLatin1String("content")).toArray());
        q.createdAt = QDateTime::fromString(obj.value(QLatin1String("created_at")).toString(),
                                            Qt::ISODateWithMs);
        return q;
    }

    static QList<ChatQueuedMessage> listFromJson(const QJsonArray& arr) {
        QList<ChatQueuedMessage> list;
        list.reserve(arr.size());
        for (const QJsonValue& v : arr) list.append(fromJson(v.toObject()));
        return list;
    }
};

Q_DECLARE_METATYPE(ChatQueuedMessage)

#endif  // CODER_DTO_CHATMESSAGE_H
