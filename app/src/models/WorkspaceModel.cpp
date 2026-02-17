#include "models/WorkspaceModel.h"
#include "api/dto/Workspace.h"

#include <QDebug>
#include <QJsonArray>
#include <QJsonValue>

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

WorkspaceModel::WorkspaceModel(QObject* parent) : QAbstractListModel(parent) {}

// ---------------------------------------------------------------------------
// QAbstractListModel interface
// ---------------------------------------------------------------------------

int WorkspaceModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) return 0;
    return static_cast<int>(m_workspaces.size());
}

QVariant WorkspaceModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(m_workspaces.size()))
        return {};

    const WorkspaceInfo& ws = m_workspaces.at(index.row());

    switch (static_cast<Roles>(role)) {
        case IdRole:
            return ws.id;
        case NameRole:
            return ws.name;
        case OwnerNameRole:
            return ws.ownerName;
        case TemplateNameRole:
            return ws.templateName;
        case TemplateIconRole:
            return ws.templateIcon;
        case StatusRole:
            return ws.status;
        case StatusStringRole:
            return statusToString(ws.status);
        case HealthRole:
            return ws.health;
        case FavoriteRole:
            return ws.favorite;
        case OutdatedRole:
            return ws.outdated;
        case AgentCountRole:
            return ws.agentCount;
        case AgentStatusRole:
            return ws.agentStatus;
        case LastUsedAtRole:
            return ws.lastUsedAt;
    }
    return {};
}

QHash<int, QByteArray> WorkspaceModel::roleNames() const {
    return {
        {IdRole, "id"},
        {NameRole, "name"},
        {OwnerNameRole, "ownerName"},
        {TemplateNameRole, "templateName"},
        {TemplateIconRole, "templateIcon"},
        {StatusRole, "status"},
        {StatusStringRole, "statusString"},
        {HealthRole, "health"},
        {FavoriteRole, "favorite"},
        {OutdatedRole, "outdated"},
        {AgentCountRole, "agentCount"},
        {AgentStatusRole, "agentStatus"},
        {LastUsedAtRole, "lastUsedAt"},
    };
}

// ---------------------------------------------------------------------------
// Data management
// ---------------------------------------------------------------------------

void WorkspaceModel::setWorkspaces(const QList<WorkspaceInfo>& workspaces) {
    int removed = 0, updated = 0, added = 0;

    // --- Handle removed items ---
    // Remove items that are no longer in the new list.
    QSet<QString> newIds;
    newIds.reserve(workspaces.size());
    for (const auto& ws : workspaces) newIds.insert(ws.id);

    for (int i = m_workspaces.size() - 1; i >= 0; --i) {
        if (!newIds.contains(m_workspaces[i].id)) {
            beginRemoveRows(QModelIndex(), i, i);
            m_workspaces.removeAt(i);
            endRemoveRows();
            ++removed;
        }
    }

    // --- Handle updates and additions ---
    // Build a map of current positions for O(1) lookup.
    QHash<QString, int> currentPositions;
    currentPositions.reserve(m_workspaces.size());
    for (int i = 0; i < m_workspaces.size(); ++i) currentPositions[m_workspaces[i].id] = i;

    for (int i = 0; i < workspaces.size(); ++i) {
        const auto& ws = workspaces[i];
        if (currentPositions.contains(ws.id)) {
            // Existing item — update in-place (no delegate destruction).
            const int row = currentPositions[ws.id];
            m_workspaces[row] = ws;
            emit dataChanged(index(row), index(row));
            ++updated;
        } else {
            // New item — append.
            const int row = m_workspaces.size();
            beginInsertRows(QModelIndex(), row, row);
            m_workspaces.append(ws);
            endInsertRows();
            ++added;
        }
    }

    qDebug() << "[WorkspaceModel] setWorkspaces: input=" << workspaces.size()
             << "removed=" << removed << "updated=" << updated << "added=" << added
             << "total=" << m_workspaces.size();

    emit countChanged();
}

void WorkspaceModel::updateWorkspace(const WorkspaceInfo& workspace) {
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

void WorkspaceModel::clear() {
    if (m_workspaces.isEmpty()) return;
    beginResetModel();
    m_workspaces.clear();
    endResetModel();
    emit countChanged();
}

// ---------------------------------------------------------------------------
// Observable state
// ---------------------------------------------------------------------------

bool WorkspaceModel::isLoading() const {
    return m_loading;
}

void WorkspaceModel::setLoading(bool loading) {
    if (m_loading == loading) return;
    m_loading = loading;
    emit loadingChanged();
}

QString WorkspaceModel::errorMessage() const {
    return m_errorMessage;
}

void WorkspaceModel::setErrorMessage(const QString& msg) {
    if (m_errorMessage == msg) return;
    m_errorMessage = msg;
    emit errorChanged();
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

QString WorkspaceModel::statusToString(int status) {
    // Maps WorkspaceStatus enum values to display strings.
    switch (static_cast<WorkspaceStatus>(status)) {
        case WorkspaceStatus::Running:
            return QStringLiteral("Running");
        case WorkspaceStatus::Stopped:
            return QStringLiteral("Stopped");
        case WorkspaceStatus::Starting:
            return QStringLiteral("Starting");
        case WorkspaceStatus::Stopping:
            return QStringLiteral("Stopping");
        case WorkspaceStatus::Failed:
            return QStringLiteral("Failed");
        case WorkspaceStatus::Canceling:
            return QStringLiteral("Canceling");
        case WorkspaceStatus::Canceled:
            return QStringLiteral("Canceled");
        case WorkspaceStatus::Deleting:
            return QStringLiteral("Deleting");
        case WorkspaceStatus::Deleted:
            return QStringLiteral("Deleted");
        case WorkspaceStatus::Pending:
            return QStringLiteral("Pending");
        case WorkspaceStatus::Unknown:
            return QStringLiteral("Unknown");
    }
    return QStringLiteral("Unknown");
}

// ---------------------------------------------------------------------------
// JSON parsing
// ---------------------------------------------------------------------------

WorkspaceModel::WorkspaceInfo WorkspaceModel::WorkspaceInfo::fromJson(const QJsonObject& obj) {
    // Delegate to the full Workspace DTO parser which correctly handles
    // the nested latest_build.status string and health object.
    const Workspace ws = Workspace::fromJson(obj);

    WorkspaceInfo info;
    info.id = ws.id;
    info.name = ws.name;
    info.ownerName = ws.ownerName;
    info.templateName = ws.templateName;
    info.templateIcon = ws.templateIcon;
    info.status = static_cast<int>(ws.status);
    info.health = (ws.health == QLatin1String("healthy"));
    info.favorite = ws.favorite;
    info.outdated = ws.outdated;
    info.agentCount = static_cast<int>(ws.agents.size());
    info.agentStatus = ws.agents.isEmpty() ? 0 : static_cast<int>(ws.agents.first().status);

    const QString ts = obj.value(QLatin1String("last_used_at")).toString();
    if (!ts.isEmpty()) info.lastUsedAt = QDateTime::fromString(ts, Qt::ISODate);

    return info;
}
