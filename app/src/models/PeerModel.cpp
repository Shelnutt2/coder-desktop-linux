#include "models/PeerModel.h"

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

PeerModel::PeerModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

// ---------------------------------------------------------------------------
// QAbstractListModel interface
// ---------------------------------------------------------------------------

int PeerModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;
    return static_cast<int>(m_peers.size());
}

QVariant PeerModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0
        || index.row() >= static_cast<int>(m_peers.size()))
        return {};

    const PeerInfo& peer = m_peers.at(index.row());

    switch (static_cast<Roles>(role)) {
    case WorkspaceNameRole: return peer.workspaceName;
    case AgentNameRole:     return peer.agentName;
    case HostnameRole:      return peer.hostname;
    case StatusRole:        return peer.status;
    case StatusStringRole:  return statusToString(peer.status);
    case LastPingMsRole:    return peer.lastPingMs;
    case IsP2PRole:         return peer.isP2P;
    case ConnectionTypeRole:
        return peer.isP2P ? QStringLiteral("P2P")
                          : QStringLiteral("Relayed");
    }
    return {};
}

QHash<int, QByteArray> PeerModel::roleNames() const
{
    return {
        {WorkspaceNameRole, "workspaceName"},
        {AgentNameRole,     "agentName"},
        {HostnameRole,      "hostname"},
        {StatusRole,        "status"},
        {StatusStringRole,  "statusString"},
        {LastPingMsRole,    "lastPingMs"},
        {IsP2PRole,         "isP2P"},
        {ConnectionTypeRole,"connectionType"},
    };
}

// ---------------------------------------------------------------------------
// Data management
// ---------------------------------------------------------------------------

void PeerModel::setPeers(const QList<PeerInfo>& peers)
{
    beginResetModel();
    m_peers = peers;
    endResetModel();
    emit countChanged();
}

void PeerModel::updatePeer(const PeerInfo& peer)
{
    for (int i = 0; i < static_cast<int>(m_peers.size()); ++i) {
        if (m_peers[i].hostname == peer.hostname) {
            m_peers[i] = peer;
            emit dataChanged(index(i), index(i));
            return;
        }
    }

    // Not found — append as new.
    const int row = static_cast<int>(m_peers.size());
    beginInsertRows(QModelIndex(), row, row);
    m_peers.append(peer);
    endInsertRows();
    emit countChanged();
}

void PeerModel::clear()
{
    if (m_peers.isEmpty())
        return;
    beginResetModel();
    m_peers.clear();
    endResetModel();
    emit countChanged();
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

QString PeerModel::statusToString(int status)
{
    switch (status) {
    case 0: return QStringLiteral("Disconnected");
    case 1: return QStringLiteral("Connecting");
    case 2: return QStringLiteral("Connected");
    }
    return QStringLiteral("Unknown");
}
