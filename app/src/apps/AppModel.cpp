#include "AppModel.h"

#include <algorithm>

AppModel::AppModel(QObject* parent) : QAbstractListModel(parent) {
    connect(&m_discovery, &AppDiscovery::scanComplete, this, [this](const QVector<AppEntry>& apps) {
        m_allApps = apps;

        // Sort by category then by name (case-insensitive).
        std::sort(m_allApps.begin(), m_allApps.end(), [](const AppEntry& a, const AppEntry& b) {
            int cmp = a.category.compare(b.category, Qt::CaseInsensitive);
            if (cmp != 0) {
                return cmp < 0;
            }
            return a.name.compare(b.name, Qt::CaseInsensitive) < 0;
        });

        applyFilters();

        m_loading = false;
        emit loadingChanged();
        emit totalCountChanged();
    });
}

int AppModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) {
        return 0;
    }
    return static_cast<int>(m_filteredApps.size());
}

QVariant AppModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= m_filteredApps.size()) {
        return {};
    }

    const auto& app = m_filteredApps.at(index.row());
    switch (role) {
        case NameRole:
            return app.name;
        case ExecRole:
            return app.exec;
        case IconNameRole:
            return app.iconName;
        case IconPathRole:
            return app.iconPath;
        case CategoryRole:
            return app.category;
        case IdRole:
            return app.id;
        case FromDesktopFileRole:
            return app.fromDesktopFile;
        default:
            return {};
    }
}

QHash<int, QByteArray> AppModel::roleNames() const {
    return {
        {NameRole, "name"},
        {ExecRole, "exec"},
        {IconNameRole, "iconName"},
        {IconPathRole, "iconPath"},
        {CategoryRole, "category"},
        {IdRole, "appId"},
        {FromDesktopFileRole, "fromDesktopFile"},
    };
}

QString AppModel::searchQuery() const {
    return m_searchQuery;
}

void AppModel::setSearchQuery(const QString& query) {
    if (m_searchQuery == query) {
        return;
    }
    m_searchQuery = query;
    emit searchQueryChanged();
    applyFilters();
}

QString AppModel::categoryFilter() const {
    return m_categoryFilter;
}

void AppModel::setCategoryFilter(const QString& category) {
    if (m_categoryFilter == category) {
        return;
    }
    m_categoryFilter = category;
    emit categoryFilterChanged();
    applyFilters();
}

bool AppModel::isLoading() const {
    return m_loading;
}

int AppModel::totalCount() const {
    return static_cast<int>(m_allApps.size());
}

void AppModel::refresh() {
    m_loading = true;
    emit loadingChanged();
    m_discovery.scanAsync();
}

void AppModel::applyFilters() {
    beginResetModel();

    m_filteredApps.clear();
    for (const auto& app : m_allApps) {
        // Category filter (empty = show all).
        if (!m_categoryFilter.isEmpty() && app.category != m_categoryFilter) {
            continue;
        }
        // Search filter: case-insensitive substring on name, exec, or category.
        if (!m_searchQuery.isEmpty()) {
            bool match = app.name.contains(m_searchQuery, Qt::CaseInsensitive) ||
                         app.exec.contains(m_searchQuery, Qt::CaseInsensitive) ||
                         app.category.contains(m_searchQuery, Qt::CaseInsensitive);
            if (!match) {
                continue;
            }
        }
        m_filteredApps.append(app);
    }

    endResetModel();
}
