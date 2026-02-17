#ifndef CODER_DTO_WORKSPACE_H
#define CODER_DTO_WORKSPACE_H

#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QMetaType>
#include <QString>

// ---------------------------------------------------------------------------
// Enums
// ---------------------------------------------------------------------------

enum class WorkspaceStatus {
    Running,
    Stopped,
    Starting,
    Stopping,
    Failed,
    Canceling,
    Canceled,
    Deleting,
    Deleted,
    Pending,
    Unknown,
};

enum class AgentStatus {
    Connected,
    Disconnected,
    Connecting,
    Timeout,
    Unknown,
};

// ---------------------------------------------------------------------------
// Helper: string → enum conversions
// ---------------------------------------------------------------------------

inline WorkspaceStatus workspaceStatusFromString(const QString& s) {
    if (s == QLatin1String("running")) return WorkspaceStatus::Running;
    if (s == QLatin1String("stopped")) return WorkspaceStatus::Stopped;
    if (s == QLatin1String("starting")) return WorkspaceStatus::Starting;
    if (s == QLatin1String("stopping")) return WorkspaceStatus::Stopping;
    if (s == QLatin1String("failed")) return WorkspaceStatus::Failed;
    if (s == QLatin1String("canceling")) return WorkspaceStatus::Canceling;
    if (s == QLatin1String("canceled")) return WorkspaceStatus::Canceled;
    if (s == QLatin1String("deleting")) return WorkspaceStatus::Deleting;
    if (s == QLatin1String("deleted")) return WorkspaceStatus::Deleted;
    if (s == QLatin1String("pending")) return WorkspaceStatus::Pending;
    return WorkspaceStatus::Unknown;
}

inline AgentStatus agentStatusFromString(const QString& s) {
    if (s == QLatin1String("connected")) return AgentStatus::Connected;
    if (s == QLatin1String("disconnected")) return AgentStatus::Disconnected;
    if (s == QLatin1String("connecting")) return AgentStatus::Connecting;
    if (s == QLatin1String("timeout")) return AgentStatus::Timeout;
    return AgentStatus::Unknown;
}

// ---------------------------------------------------------------------------
// WorkspaceApp
// ---------------------------------------------------------------------------

struct WorkspaceApp {
    Q_GADGET
    Q_PROPERTY(QString displayName MEMBER displayName)
    Q_PROPERTY(QString slug MEMBER slug)
    Q_PROPERTY(QString icon MEMBER icon)
    Q_PROPERTY(QString url MEMBER url)
    Q_PROPERTY(bool subdomain MEMBER subdomain)
    Q_PROPERTY(bool external MEMBER external)

public:
    QString displayName;
    QString slug;
    QString icon;
    QString url;
    bool subdomain = false;
    bool external = false;

    static WorkspaceApp fromJson(const QJsonObject& obj) {
        WorkspaceApp a;
        a.displayName = obj.value(QLatin1String("display_name")).toString();
        a.slug = obj.value(QLatin1String("slug")).toString();
        a.icon = obj.value(QLatin1String("icon")).toString();
        a.url = obj.value(QLatin1String("url")).toString();
        a.subdomain = obj.value(QLatin1String("subdomain")).toBool();
        a.external = obj.value(QLatin1String("external")).toBool();
        return a;
    }

    static QList<WorkspaceApp> listFromJson(const QJsonArray& arr) {
        QList<WorkspaceApp> list;
        list.reserve(arr.size());
        for (const QJsonValue& v : arr) list.append(fromJson(v.toObject()));
        return list;
    }
};

Q_DECLARE_METATYPE(WorkspaceApp)

// ---------------------------------------------------------------------------
// Agent
// ---------------------------------------------------------------------------

struct Agent {
    Q_GADGET
    Q_PROPERTY(QString id MEMBER id)
    Q_PROPERTY(QString name MEMBER name)

public:
    QString id;
    QString name;
    AgentStatus status = AgentStatus::Unknown;
    QList<WorkspaceApp> apps;

    static Agent fromJson(const QJsonObject& obj) {
        Agent a;
        a.id = obj.value(QLatin1String("id")).toString();
        a.name = obj.value(QLatin1String("name")).toString();
        a.status = agentStatusFromString(obj.value(QLatin1String("status")).toString());
        a.apps = WorkspaceApp::listFromJson(obj.value(QLatin1String("apps")).toArray());
        return a;
    }

    static QList<Agent> listFromJson(const QJsonArray& arr) {
        QList<Agent> list;
        list.reserve(arr.size());
        for (const QJsonValue& v : arr) list.append(fromJson(v.toObject()));
        return list;
    }
};

Q_DECLARE_METATYPE(Agent)

// ---------------------------------------------------------------------------
// Workspace
// ---------------------------------------------------------------------------

struct Workspace {
    Q_GADGET
    Q_PROPERTY(QString id MEMBER id)
    Q_PROPERTY(QString name MEMBER name)
    Q_PROPERTY(QString ownerName MEMBER ownerName)
    Q_PROPERTY(QString templateName MEMBER templateName)
    Q_PROPERTY(QString templateIcon MEMBER templateIcon)
    Q_PROPERTY(QString health MEMBER health)
    Q_PROPERTY(bool favorite MEMBER favorite)
    Q_PROPERTY(bool outdated MEMBER outdated)
    Q_PROPERTY(QString latestBuildId MEMBER latestBuildId)
    Q_PROPERTY(QString templateActiveVersionId MEMBER templateActiveVersionId)

public:
    QString id;
    QString name;
    QString ownerName;
    QString templateName;
    QString templateIcon;
    WorkspaceStatus status = WorkspaceStatus::Unknown;
    QString health;
    QList<Agent> agents;
    bool favorite = false;
    bool outdated = false;
    QString latestBuildId;
    QString templateActiveVersionId;

    static Workspace fromJson(const QJsonObject& obj) {
        Workspace w;
        w.id = obj.value(QLatin1String("id")).toString();
        w.name = obj.value(QLatin1String("name")).toString();
        w.ownerName = obj.value(QLatin1String("owner_name")).toString();
        w.templateName = obj.value(QLatin1String("template_name")).toString();
        w.templateIcon = obj.value(QLatin1String("template_icon")).toString();
        w.health =
            obj.value(QLatin1String("health")).toObject().value(QLatin1String("healthy")).toBool()
                ? QStringLiteral("healthy")
                : QStringLiteral("unhealthy");
        w.favorite = obj.value(QLatin1String("favorite")).toBool();
        w.outdated = obj.value(QLatin1String("outdated")).toBool();

        // Status comes from latest_build.status
        const QJsonObject latestBuild = obj.value(QLatin1String("latest_build")).toObject();
        w.latestBuildId = latestBuild.value(QLatin1String("id")).toString();
        w.status = workspaceStatusFromString(latestBuild.value(QLatin1String("status")).toString());
        w.templateActiveVersionId =
            obj.value(QLatin1String("template_active_version_id")).toString();

        // Agents are nested inside latest_build → resources[] → agents[]
        const QJsonArray resources = latestBuild.value(QLatin1String("resources")).toArray();
        for (const QJsonValue& rv : resources) {
            const QJsonArray agentsArr = rv.toObject().value(QLatin1String("agents")).toArray();
            for (const QJsonValue& av : agentsArr) w.agents.append(Agent::fromJson(av.toObject()));
        }
        return w;
    }

    static QList<Workspace> listFromJson(const QJsonArray& arr) {
        QList<Workspace> list;
        list.reserve(arr.size());
        for (const QJsonValue& v : arr) list.append(fromJson(v.toObject()));
        return list;
    }
};

Q_DECLARE_METATYPE(Workspace)

#endif  // CODER_DTO_WORKSPACE_H
