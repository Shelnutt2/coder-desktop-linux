#ifndef PEERMODEL_H
#define PEERMODEL_H

#include <QAbstractListModel>
#include <QString>

/// List model exposing VPN peer information to QML views.
///
/// Each row represents one Coder workspace agent reachable over the VPN
/// tunnel.  Data is pushed from C++ via setPeers() / updatePeer() / clear().
class PeerModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    enum Roles {
        WorkspaceNameRole = Qt::UserRole + 1,
        AgentNameRole,
        HostnameRole,
        StatusRole,
        StatusStringRole,
        LastPingMsRole,
        IsP2PRole,
        ConnectionTypeRole,
    };
    Q_ENUM(Roles)

    /// Per-peer data carried by the model.
    struct PeerInfo {
        QString workspaceName;
        QString agentName;
        QString hostname;       ///< e.g. "myworkspace.coder"
        int status = 0;        ///< 0=disconnected, 1=connecting, 2=connected
        qint64 lastPingMs = -1; ///< -1 if unknown
        bool isP2P = false;     ///< true=direct, false=relayed
    };

    explicit PeerModel(QObject* parent = nullptr);

    // -- QAbstractListModel interface --
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    // -- Data management --
    void setPeers(const QList<PeerInfo>& peers);
    void updatePeer(const PeerInfo& peer);  ///< Update by hostname match.
    void clear();

signals:
    void countChanged();

private:
    static QString statusToString(int status);

    QList<PeerInfo> m_peers;
};

#endif // PEERMODEL_H
