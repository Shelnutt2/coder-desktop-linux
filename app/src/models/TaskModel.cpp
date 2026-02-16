#include "models/TaskModel.h"

#include <QJsonArray>
#include <QJsonValue>

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

TaskModel::TaskModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

// ---------------------------------------------------------------------------
// QAbstractListModel interface
// ---------------------------------------------------------------------------

int TaskModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return static_cast<int>(m_tasks.size());
}

QVariant TaskModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0
        || index.row() >= static_cast<int>(m_tasks.size()))
        return {};

    const TaskInfo &task = m_tasks.at(index.row());

    switch (static_cast<Roles>(role)) {
    case IdRole:                return task.id;
    case NameRole:              return task.name;
    case DisplayNameRole:       return task.displayName;
    case OwnerNameRole:         return task.ownerName;
    case OwnerAvatarRole:       return task.ownerAvatarUrl;
    case TemplateNameRole:      return task.templateName;
    case TemplateIconRole:      return task.templateIcon;
    case WorkspaceNameRole:     return task.workspaceName;
    case InitialPromptRole:     return task.initialPrompt;
    case StatusRole:            return task.status;
    case StatusStringRole:      return taskStatusToString(static_cast<TaskStatus>(task.status));
    case CurrentStateRole:      return task.currentState;
    case CurrentStateMessageRole: return task.currentStateMessage;
    case CreatedAtRole:         return task.createdAt;
    case UpdatedAtRole:         return task.updatedAt;
    }
    return {};
}

QHash<int, QByteArray> TaskModel::roleNames() const
{
    return {
        {IdRole,                "id"},
        {NameRole,              "name"},
        {DisplayNameRole,       "displayName"},
        {OwnerNameRole,         "ownerName"},
        {OwnerAvatarRole,       "ownerAvatar"},
        {TemplateNameRole,      "templateName"},
        {TemplateIconRole,      "templateIcon"},
        {WorkspaceNameRole,     "workspaceName"},
        {InitialPromptRole,     "initialPrompt"},
        {StatusRole,            "status"},
        {StatusStringRole,      "statusString"},
        {CurrentStateRole,      "currentState"},
        {CurrentStateMessageRole, "currentStateMessage"},
        {CreatedAtRole,         "createdAt"},
        {UpdatedAtRole,         "updatedAt"},
    };
}

// ---------------------------------------------------------------------------
// Data management
// ---------------------------------------------------------------------------

void TaskModel::setTasks(const QList<TaskInfo> &tasks)
{
    // --- Handle removed items ---
    QSet<QString> newIds;
    newIds.reserve(tasks.size());
    for (const auto &t : tasks)
        newIds.insert(t.id);

    for (int i = m_tasks.size() - 1; i >= 0; --i) {
        if (!newIds.contains(m_tasks[i].id)) {
            beginRemoveRows(QModelIndex(), i, i);
            m_tasks.removeAt(i);
            endRemoveRows();
        }
    }

    // --- Handle updates and additions ---
    QHash<QString, int> currentPositions;
    currentPositions.reserve(m_tasks.size());
    for (int i = 0; i < m_tasks.size(); ++i)
        currentPositions[m_tasks[i].id] = i;

    for (int i = 0; i < tasks.size(); ++i) {
        const auto &task = tasks[i];
        if (currentPositions.contains(task.id)) {
            const int row = currentPositions[task.id];
            m_tasks[row] = task;
            emit dataChanged(index(row), index(row));
        } else {
            const int row = m_tasks.size();
            beginInsertRows(QModelIndex(), row, row);
            m_tasks.append(task);
            endInsertRows();
        }
    }

    emit countChanged();
}

void TaskModel::updateTask(const TaskInfo &task)
{
    for (int i = 0; i < static_cast<int>(m_tasks.size()); ++i) {
        if (m_tasks[i].id == task.id) {
            m_tasks[i] = task;
            emit dataChanged(index(i), index(i));
            return;
        }
    }

    // Not found — append as new.
    const int row = static_cast<int>(m_tasks.size());
    beginInsertRows(QModelIndex(), row, row);
    m_tasks.append(task);
    endInsertRows();
    emit countChanged();
}

void TaskModel::clear()
{
    if (m_tasks.isEmpty())
        return;
    beginResetModel();
    m_tasks.clear();
    endResetModel();
    emit countChanged();
}

// ---------------------------------------------------------------------------
// Observable state
// ---------------------------------------------------------------------------

bool TaskModel::isLoading() const { return m_loading; }

void TaskModel::setLoading(bool loading)
{
    if (m_loading == loading)
        return;
    m_loading = loading;
    emit loadingChanged();
}

QString TaskModel::errorMessage() const { return m_errorMessage; }

void TaskModel::setErrorMessage(const QString &msg)
{
    if (m_errorMessage == msg)
        return;
    m_errorMessage = msg;
    emit errorChanged();
}

// ---------------------------------------------------------------------------
// JSON parsing
// ---------------------------------------------------------------------------

TaskModel::TaskInfo TaskModel::TaskInfo::fromJson(const QJsonObject &obj)
{
    const Task task = Task::fromJson(obj);
    return fromTask(task);
}

TaskModel::TaskInfo TaskModel::TaskInfo::fromTask(const Task &task)
{
    TaskInfo info;
    info.id                  = task.id;
    info.name                = task.name;
    info.displayName         = task.displayName;
    info.ownerName           = task.ownerName;
    info.ownerAvatarUrl      = task.ownerAvatarUrl;
    info.templateName        = task.templateName;
    info.templateIcon        = task.templateIcon;
    info.workspaceName       = task.workspaceName;
    info.initialPrompt       = task.initialPrompt;
    info.status              = static_cast<int>(task.status);
    info.currentStateMessage = task.currentState.message;
    info.currentState        = static_cast<int>(task.currentState.state);
    info.createdAt           = task.createdAt;
    info.updatedAt           = task.updatedAt;
    return info;
}
