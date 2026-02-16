#include "models/WorkspaceModel.h"

#include <QJsonArray>
#include <QJsonValue>

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

WorkspaceModel::WorkspaceModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

// ---------------------------------------------------------------------------
// QAbstractListModel interface
// ---------------------------------------------------------------------------

int WorkspaceModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;
    return static_cast<int>(m_workspaces.size());
}

QVariant WorkspaceModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0
        || index.row() >= static_cast<int>(m_workspaces.size()))
        return {};

    const WorkspaceInfo& ws = m_workspaces.at(index.row());

    switch (static_cast<Roles>(role)) {
    case IdRole:           return ws.id;
    case NameRole:         return ws.name;
    case OwnerNameRole:    return ws.ownerName;
    case TemplateNameRole: return ws.templateName;
    case TemplateIconRole: return ws.templateIcon;
    case StatusRole:       return ws.status;
    case StatusStringRole: return statusToString(ws.status);
    case HealthRole:       return ws.health;
    case FavoriteRole:     return ws.favorite;
    case OutdatedRole:     return ws.outdated;
    case AgentCountRole:   return ws.agentCount;
    case AgentStatusRole:  return ws.agentStatus;
    case LastUsedAtRole:   return ws.lastUsedAt;
    }
    return {};
}

QHash<int, QByteArray> WorkspaceModel::roleNames() const
{
    return {
        {IdRole,           "id"},
        {NameRole,         "name"},
        {OwnerNameRole,    "ownerName"},
        {TemplateNameRole, "templateName"},
        {TemplateIconRole, "templateIcon"},
        {StatusRole,       "status"},
        {StatusStringRole, "statusString"},
        {HealthRole,       "health"},
        {FavoriteRole,     "favorite"},
        {OutdatedRole,     "outdated"},
        {AgentCountRole,   "agentCount"},
        {AgentStatusRole,  "agentStatus"},
        {LastUsedAtRole,   "lastUsedAt"},
    };
}

// ---------------------------------------------------------------------------
// Data management
// ---------------------------------------------------------------------------

void WorkspaceModel::setWorkspaces(const QList<WorkspaceInfo>& workspaces)
{
    beginResetModel();
    m_workspaces = workspaces;
    endResetModel();
    emit countChanged();
}

void WorkspaceModel::updateWorkspace(const WorkspaceInfo& workspace)
{
    for (int i = 0; i < static_cast<int>(m_workspaces.size()); ++i) {
        if (m_workspaces[i].id == workspace.id) {
            m_workspaces[i] = workspace;
            emit dataChanged(index(i), index(i));
            return;
        }
    }

    // Not found — append as new.
    const int row = static_cast<int>(m_workspaces.size());
    beginInsertRows(QModelIndex(), row, row);
    m_workspaces.append(workspace);
    endInsertRows();
    emit countChanged();
}

void WorkspaceModel::clear()
{
    if (m_workspaces.isEmpty())
        return;
    beginResetModel();
    m_workspaces.clear();
    endResetModel();
    emit countChanged();
}

// ---------------------------------------------------------------------------
// Observable state
// ---------------------------------------------------------------------------

bool WorkspaceModel::isLoading() const { return m_loading; }

void WorkspaceModel::setLoading(bool loading)
{
    if (m_loading == loading)
        return;
    m_loading = loading;
    emit loadingChanged();
}

QString WorkspaceModel::errorMessage() const { return m_errorMessage; }

void WorkspaceModel::setErrorMessage(const QString& msg)
{
    if (m_errorMessage == msg)
        return;
    m_errorMessage = msg;
    emit errorChanged();
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

QString WorkspaceModel::statusToString(int status)
{
    // Mirrors typical Coder workspace statuses.
    switch (status) {
    case 0: return QStringLiteral("Unknown");
    case 1: return QStringLiteral("Starting");
    case 2: return QStringLiteral("Running");
    case 3: return QStringLiteral("Stopping");
    case 4: return QStringLiteral("Stopped");
    case 5: return QStringLiteral("Failed");
    case 6: return QStringLiteral("Deleting");
    case 7: return QStringLiteral("Deleted");
    case 8: return QStringLiteral("Canceling");
    case 9: return QStringLiteral("Canceled");
    }
    return QStringLiteral("Unknown");
}

// ---------------------------------------------------------------------------
// JSON parsing
// ---------------------------------------------------------------------------

WorkspaceModel::WorkspaceInfo WorkspaceModel::WorkspaceInfo::fromJson(
    const QJsonObject& obj)
{
    WorkspaceInfo info;
    info.id           = obj.value(QLatin1String("id")).toString();
    info.name         = obj.value(QLatin1String("name")).toString();
    info.ownerName    = obj.value(QLatin1String("owner_name")).toString();
    info.templateName = obj.value(QLatin1String("template_name")).toString();
    info.templateIcon = obj.value(QLatin1String("template_icon")).toString();
    info.status       = obj.value(QLatin1String("status")).toInt(0);
    info.health       = obj.value(QLatin1String("health")).toBool(true);
    info.favorite     = obj.value(QLatin1String("favorite")).toBool(false);
    info.outdated     = obj.value(QLatin1String("outdated")).toBool(false);
    info.agentCount   = obj.value(QLatin1String("agent_count")).toInt(0);
    info.agentStatus  = obj.value(QLatin1String("agent_status")).toInt(0);

    const QString ts = obj.value(QLatin1String("last_used_at")).toString();
    if (!ts.isEmpty())
        info.lastUsedAt = QDateTime::fromString(ts, Qt::ISODate);

    return info;
}
