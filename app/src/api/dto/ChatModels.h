#ifndef CODER_DTO_CHATMODELS_H
#define CODER_DTO_CHATMODELS_H

#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QMetaType>
#include <QString>

// Model catalog, model config, MCP server, and usage-limit DTOs for the
// experimental Coder Agents chat API. JSON field names match
// coder/coder codersdk/chats.go and codersdk/mcp.go exactly.

// ---------------------------------------------------------------------------
// ChatModel / ChatModelProvider (GET /api/experimental/chats/models)
// ---------------------------------------------------------------------------

struct ChatModel {
    QString id;
    QString provider;
    QString model;
    QString displayName;

    static ChatModel fromJson(const QJsonObject& obj) {
        ChatModel m;
        m.id = obj.value(QLatin1String("id")).toString();
        m.provider = obj.value(QLatin1String("provider")).toString();
        m.model = obj.value(QLatin1String("model")).toString();
        m.displayName = obj.value(QLatin1String("display_name")).toString();
        return m;
    }

    static QList<ChatModel> listFromJson(const QJsonArray& arr) {
        QList<ChatModel> list;
        list.reserve(arr.size());
        for (const QJsonValue& v : arr) list.append(fromJson(v.toObject()));
        return list;
    }
};

Q_DECLARE_METATYPE(ChatModel)

struct ChatModelProvider {
    QString provider;
    bool available = false;
    QString unavailableReason;  // missing_api_key|fetch_failed|user_api_key_required
    QList<ChatModel> models;

    static ChatModelProvider fromJson(const QJsonObject& obj) {
        ChatModelProvider p;
        p.provider = obj.value(QLatin1String("provider")).toString();
        p.available = obj.value(QLatin1String("available")).toBool();
        p.unavailableReason = obj.value(QLatin1String("unavailable_reason")).toString();
        p.models = ChatModel::listFromJson(obj.value(QLatin1String("models")).toArray());
        return p;
    }
};

Q_DECLARE_METATYPE(ChatModelProvider)

struct ChatModelsResponse {
    QList<ChatModelProvider> providers;

    /// True when at least one provider is usable.
    [[nodiscard]] bool anyAvailable() const {
        for (const ChatModelProvider& p : providers) {
            if (p.available) return true;
        }
        return false;
    }

    static ChatModelsResponse fromJson(const QJsonObject& obj) {
        ChatModelsResponse r;
        const QJsonArray arr = obj.value(QLatin1String("providers")).toArray();
        r.providers.reserve(arr.size());
        for (const QJsonValue& v : arr)
            r.providers.append(ChatModelProvider::fromJson(v.toObject()));
        return r;
    }
};

Q_DECLARE_METATYPE(ChatModelsResponse)

// ---------------------------------------------------------------------------
// ChatModelConfig (GET /api/experimental/chats/model-configs)
//
// model_config_id in requests is the UUID of one of these entries,
// not a catalog model slug.
// ---------------------------------------------------------------------------

struct ChatModelConfig {
    QString id;  // UUID
    QString aiProviderId;
    QString model;
    QString displayName;
    bool enabled = false;
    bool isDefault = false;
    qint64 contextLimit = -1;  // -1 when absent

    static ChatModelConfig fromJson(const QJsonObject& obj) {
        ChatModelConfig c;
        c.id = obj.value(QLatin1String("id")).toString();
        c.aiProviderId = obj.value(QLatin1String("ai_provider_id")).toString();
        c.model = obj.value(QLatin1String("model")).toString();
        c.displayName = obj.value(QLatin1String("display_name")).toString();
        c.enabled = obj.value(QLatin1String("enabled")).toBool();
        c.isDefault = obj.value(QLatin1String("is_default")).toBool();
        if (obj.contains(QLatin1String("context_limit")))
            c.contextLimit = obj.value(QLatin1String("context_limit")).toVariant().toLongLong();
        return c;
    }

    static QList<ChatModelConfig> listFromJson(const QJsonArray& arr) {
        QList<ChatModelConfig> list;
        list.reserve(arr.size());
        for (const QJsonValue& v : arr) list.append(fromJson(v.toObject()));
        return list;
    }
};

Q_DECLARE_METATYPE(ChatModelConfig)

// ---------------------------------------------------------------------------
// McpServerConfig (GET /api/experimental/mcp/servers)
// ---------------------------------------------------------------------------

struct McpServerConfig {
    QString id;
    QString displayName;
    QString slug;
    QString transport;  // streamable_http|sse
    QString url;
    QString authType;  // none|oauth2|api_key|custom_headers|user_oidc
    bool authConnected = false;
    QString availability;  // force_on|default_on|default_off
    bool enabled = false;
    bool allowInPlanMode = false;

    static McpServerConfig fromJson(const QJsonObject& obj) {
        McpServerConfig s;
        s.id = obj.value(QLatin1String("id")).toString();
        s.displayName = obj.value(QLatin1String("display_name")).toString();
        s.slug = obj.value(QLatin1String("slug")).toString();
        s.transport = obj.value(QLatin1String("transport")).toString();
        s.url = obj.value(QLatin1String("url")).toString();
        s.authType = obj.value(QLatin1String("auth_type")).toString();
        s.authConnected = obj.value(QLatin1String("auth_connected")).toBool();
        s.availability = obj.value(QLatin1String("availability")).toString();
        s.enabled = obj.value(QLatin1String("enabled")).toBool();
        s.allowInPlanMode = obj.value(QLatin1String("allow_in_plan_mode")).toBool();
        return s;
    }

    static QList<McpServerConfig> listFromJson(const QJsonArray& arr) {
        QList<McpServerConfig> list;
        list.reserve(arr.size());
        for (const QJsonValue& v : arr) list.append(fromJson(v.toObject()));
        return list;
    }
};

Q_DECLARE_METATYPE(McpServerConfig)

// ---------------------------------------------------------------------------
// ChatUsageLimitStatus (GET /api/experimental/chats/usage-limits/status)
// ---------------------------------------------------------------------------

struct ChatUsageLimitStatus {
    bool isLimited = false;
    QString period;
    qint64 spendLimitMicros = -1;  // -1 when absent
    qint64 currentSpend = 0;
    QDateTime periodStart;
    QDateTime periodEnd;

    static ChatUsageLimitStatus fromJson(const QJsonObject& obj) {
        ChatUsageLimitStatus s;
        s.isLimited = obj.value(QLatin1String("is_limited")).toBool();
        s.period = obj.value(QLatin1String("period")).toString();
        if (obj.contains(QLatin1String("spend_limit_micros")))
            s.spendLimitMicros =
                obj.value(QLatin1String("spend_limit_micros")).toVariant().toLongLong();
        s.currentSpend = obj.value(QLatin1String("current_spend")).toVariant().toLongLong();
        s.periodStart = QDateTime::fromString(obj.value(QLatin1String("period_start")).toString(),
                                              Qt::ISODateWithMs);
        s.periodEnd = QDateTime::fromString(obj.value(QLatin1String("period_end")).toString(),
                                            Qt::ISODateWithMs);
        return s;
    }
};

Q_DECLARE_METATYPE(ChatUsageLimitStatus)

// ---------------------------------------------------------------------------
// ChatUsageLimitExceeded - HTTP 409 body from POST /chats/{id}/messages
// ---------------------------------------------------------------------------

struct ChatUsageLimitExceeded {
    qint64 spentMicros = 0;
    qint64 limitMicros = 0;
    QDateTime resetsAt;

    [[nodiscard]] bool isValid() const { return limitMicros > 0; }

    static ChatUsageLimitExceeded fromJson(const QJsonObject& obj) {
        ChatUsageLimitExceeded u;
        u.spentMicros = obj.value(QLatin1String("spent_micros")).toVariant().toLongLong();
        u.limitMicros = obj.value(QLatin1String("limit_micros")).toVariant().toLongLong();
        u.resetsAt = QDateTime::fromString(obj.value(QLatin1String("resets_at")).toString(),
                                           Qt::ISODateWithMs);
        return u;
    }
};

Q_DECLARE_METATYPE(ChatUsageLimitExceeded)

#endif  // CODER_DTO_CHATMODELS_H
