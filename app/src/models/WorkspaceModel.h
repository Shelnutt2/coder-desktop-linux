#ifndef WORKSPACEMODEL_H
#define WORKSPACEMODEL_H

#include <QAbstractListModel>
#include <QDateTime>
#include <QJsonObject>
#include <QString>

/// List model exposing Coder workspaces to QML views.
///
/// Data is pushed from C++ via setWorkspaces() / updateWorkspace() / clear().
/// QML binds to the roles defined in Roles and the count / loading / error
/// properties.
class WorkspaceModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    Q_PROPERTY(bool loading READ isLoading NOTIFY loadingChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorChanged)

public:
    enum Roles {
        IdRole = Qt::UserRole + 1,
        NameRole,
        OwnerNameRole,
        TemplateNameRole,
        TemplateIconRole,
        StatusRole,
        StatusStringRole,
        HealthRole,
        FavoriteRole,
        OutdatedRole,
        AgentCountRole,
        AgentStatusRole,
        LastUsedAtRole,
    };
    Q_ENUM(Roles)

    /// Simplified workspace data transferred into the model.
    struct WorkspaceInfo {
        QString id;
        QString name;
        QString ownerName;
        QString templateName;
        QString templateIcon;
        int status = 0;  // maps to a WorkspaceStatus enum value
        bool health = true;
        bool favorite = false;
        bool outdated = false;
        int agentCount = 0;
        int agentStatus = 0;
        QDateTime lastUsedAt;

        /// Parse a Coder workspace JSON object into WorkspaceInfo.
        static WorkspaceInfo fromJson(const QJsonObject& obj);
    };

    explicit WorkspaceModel(QObject* parent = nullptr);

    // -- QAbstractListModel interface --
    [[nodiscard]] int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    // -- Bulk / incremental updates (called from C++) --
    void setWorkspaces(const QList<WorkspaceInfo>& workspaces);
    void updateWorkspace(const WorkspaceInfo& workspace);
    void clear();

    // -- Observable state --
    [[nodiscard]] bool isLoading() const;
    void setLoading(bool loading);
    [[nodiscard]] QString errorMessage() const;
    void setErrorMessage(const QString& msg);

signals:
    void countChanged();
    void loadingChanged();
    void errorChanged();

private:
    static QString statusToString(int status);

    QList<WorkspaceInfo> m_workspaces;
    bool m_loading = false;
    QString m_errorMessage;
};

#endif  // WORKSPACEMODEL_H
