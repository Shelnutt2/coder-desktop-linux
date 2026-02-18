#ifndef APPMODEL_H
#define APPMODEL_H

#include <QAbstractListModel>
#include <QVector>

#include "AppDiscovery.h"
#include "AppEntry.h"

/// List model exposing discovered applications with search and category filtering.
class AppModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(QString searchQuery READ searchQuery WRITE setSearchQuery NOTIFY searchQueryChanged)
    Q_PROPERTY(QString categoryFilter READ categoryFilter WRITE setCategoryFilter NOTIFY
                   categoryFilterChanged)
    Q_PROPERTY(bool loading READ isLoading NOTIFY loadingChanged)
    Q_PROPERTY(int totalCount READ totalCount NOTIFY totalCountChanged)

public:
    enum Roles {
        NameRole = Qt::UserRole + 1,
        ExecRole,
        IconNameRole,
        IconPathRole,
        CategoryRole,
        IdRole,
        FromDesktopFileRole,
    };

    explicit AppModel(QObject* parent = nullptr);

    // QAbstractListModel
    [[nodiscard]] int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    // Properties
    [[nodiscard]] QString searchQuery() const;
    void setSearchQuery(const QString& query);
    [[nodiscard]] QString categoryFilter() const;
    void setCategoryFilter(const QString& category);
    [[nodiscard]] bool isLoading() const;
    [[nodiscard]] int totalCount() const;

    /// Start scanning for apps.
    Q_INVOKABLE void refresh();

signals:
    void searchQueryChanged();
    void categoryFilterChanged();
    void loadingChanged();
    void totalCountChanged();

private:
    void applyFilters();

    AppDiscovery m_discovery;
    QVector<AppEntry> m_allApps;       // full unfiltered list
    QVector<AppEntry> m_filteredApps;  // displayed after search + category filter
    QString m_searchQuery;
    QString m_categoryFilter;  // empty = "All"
    bool m_loading = false;
};

#endif  // APPMODEL_H
