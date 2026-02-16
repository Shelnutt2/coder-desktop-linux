#ifndef CODER_DTO_TASK_H
#define CODER_DTO_TASK_H

#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QMetaType>
#include <QString>

// ---------------------------------------------------------------------------
// Enums
// ---------------------------------------------------------------------------

enum class TaskStatus {
    Pending,
    Initializing,
    Active,
    Paused,
    Error,
    Unknown,
};

enum class TaskState {
    Working,
    Idle,
    Complete,
    Failed,
};

// ---------------------------------------------------------------------------
// Helper: string → enum conversions
// ---------------------------------------------------------------------------

inline TaskStatus taskStatusFromString(const QString &s)
{
    if (s == QLatin1String("pending"))      return TaskStatus::Pending;
    if (s == QLatin1String("initializing")) return TaskStatus::Initializing;
    if (s == QLatin1String("active"))       return TaskStatus::Active;
    if (s == QLatin1String("paused"))       return TaskStatus::Paused;
    if (s == QLatin1String("error"))        return TaskStatus::Error;
    return TaskStatus::Unknown;
}

inline TaskState taskStateFromString(const QString &s)
{
    if (s == QLatin1String("working"))  return TaskState::Working;
    if (s == QLatin1String("idle"))     return TaskState::Idle;
    if (s == QLatin1String("complete")) return TaskState::Complete;
    if (s == QLatin1String("failed"))   return TaskState::Failed;
    return TaskState::Idle;
}

inline QString taskStatusToString(TaskStatus s)
{
    switch (s) {
    case TaskStatus::Pending:      return QStringLiteral("Pending");
    case TaskStatus::Initializing: return QStringLiteral("Initializing");
    case TaskStatus::Active:       return QStringLiteral("Active");
    case TaskStatus::Paused:       return QStringLiteral("Paused");
    case TaskStatus::Error:        return QStringLiteral("Error");
    case TaskStatus::Unknown:      return QStringLiteral("Unknown");
    }
    return QStringLiteral("Unknown");
}

inline QString taskStateToString(TaskState s)
{
    switch (s) {
    case TaskState::Working:  return QStringLiteral("Working");
    case TaskState::Idle:     return QStringLiteral("Idle");
    case TaskState::Complete: return QStringLiteral("Complete");
    case TaskState::Failed:   return QStringLiteral("Failed");
    }
    return QStringLiteral("Idle");
}

// ---------------------------------------------------------------------------
// TaskStateEntry
// ---------------------------------------------------------------------------

struct TaskStateEntry {
    Q_GADGET
    Q_PROPERTY(QDateTime timestamp MEMBER timestamp)
    Q_PROPERTY(QString message MEMBER message)
    Q_PROPERTY(QString uri MEMBER uri)

public:
    QDateTime timestamp;
    TaskState state = TaskState::Idle;
    QString message;
    QString uri;

    static TaskStateEntry fromJson(const QJsonObject &obj)
    {
        TaskStateEntry e;
        const QString ts = obj.value(QLatin1String("timestamp")).toString();
        if (!ts.isEmpty())
            e.timestamp = QDateTime::fromString(ts, Qt::ISODate);
        e.state   = taskStateFromString(
            obj.value(QLatin1String("state")).toString());
        e.message = obj.value(QLatin1String("message")).toString();
        e.uri     = obj.value(QLatin1String("uri")).toString();
        return e;
    }
};

Q_DECLARE_METATYPE(TaskStateEntry)

// ---------------------------------------------------------------------------
// Task
// ---------------------------------------------------------------------------

struct Task {
    Q_GADGET
    Q_PROPERTY(QString id MEMBER id)
    Q_PROPERTY(QString name MEMBER name)
    Q_PROPERTY(QString displayName MEMBER displayName)
    Q_PROPERTY(QString ownerName MEMBER ownerName)
    Q_PROPERTY(QString ownerAvatarUrl MEMBER ownerAvatarUrl)
    Q_PROPERTY(QString templateName MEMBER templateName)
    Q_PROPERTY(QString templateDisplayName MEMBER templateDisplayName)
    Q_PROPERTY(QString templateIcon MEMBER templateIcon)
    Q_PROPERTY(QString templateVersionId MEMBER templateVersionId)
    Q_PROPERTY(QString workspaceId MEMBER workspaceId)
    Q_PROPERTY(QString workspaceName MEMBER workspaceName)
    Q_PROPERTY(QString workspaceAppId MEMBER workspaceAppId)
    Q_PROPERTY(QString initialPrompt MEMBER initialPrompt)
    Q_PROPERTY(QDateTime createdAt MEMBER createdAt)
    Q_PROPERTY(QDateTime updatedAt MEMBER updatedAt)

public:
    QString id;
    QString name;
    QString displayName;
    QString ownerName;
    QString ownerAvatarUrl;
    QString templateName;
    QString templateDisplayName;
    QString templateIcon;
    QString templateVersionId;
    QString workspaceId;
    QString workspaceName;
    QString workspaceAppId;
    QString initialPrompt;
    TaskStatus status = TaskStatus::Unknown;
    TaskStateEntry currentState;
    QDateTime createdAt;
    QDateTime updatedAt;

    static Task fromJson(const QJsonObject &obj)
    {
        Task t;
        t.id                  = obj.value(QLatin1String("id")).toString();
        t.name                = obj.value(QLatin1String("name")).toString();
        t.displayName         = obj.value(QLatin1String("display_name")).toString();
        t.ownerName           = obj.value(QLatin1String("owner_name")).toString();
        t.ownerAvatarUrl      = obj.value(QLatin1String("owner_avatar_url")).toString();
        t.templateName        = obj.value(QLatin1String("template_name")).toString();
        t.templateDisplayName = obj.value(QLatin1String("template_display_name")).toString();
        t.templateIcon        = obj.value(QLatin1String("template_icon")).toString();
        t.templateVersionId   = obj.value(QLatin1String("template_version_id")).toString();
        t.workspaceId         = obj.value(QLatin1String("workspace_id")).toString();
        t.workspaceName       = obj.value(QLatin1String("workspace_name")).toString();
        t.workspaceAppId      = obj.value(QLatin1String("workspace_app_id")).toString();
        t.initialPrompt       = obj.value(QLatin1String("initial_prompt")).toString();
        t.status              = taskStatusFromString(
            obj.value(QLatin1String("status")).toString());

        const QJsonObject stateObj =
            obj.value(QLatin1String("current_state")).toObject();
        if (!stateObj.isEmpty())
            t.currentState = TaskStateEntry::fromJson(stateObj);

        const QString created = obj.value(QLatin1String("created_at")).toString();
        if (!created.isEmpty())
            t.createdAt = QDateTime::fromString(created, Qt::ISODate);

        const QString updated = obj.value(QLatin1String("updated_at")).toString();
        if (!updated.isEmpty())
            t.updatedAt = QDateTime::fromString(updated, Qt::ISODate);

        return t;
    }

    static QList<Task> listFromJson(const QJsonArray &arr)
    {
        QList<Task> list;
        list.reserve(arr.size());
        for (const QJsonValue &v : arr)
            list.append(fromJson(v.toObject()));
        return list;
    }

    static TaskStatus statusFromString(const QString &s)
    {
        return taskStatusFromString(s);
    }

    static TaskState stateFromString(const QString &s)
    {
        return taskStateFromString(s);
    }

    static QString statusToString(TaskStatus s)
    {
        return taskStatusToString(s);
    }

    static QString stateToString(TaskState s)
    {
        return taskStateToString(s);
    }
};

Q_DECLARE_METATYPE(Task)

#endif // CODER_DTO_TASK_H
