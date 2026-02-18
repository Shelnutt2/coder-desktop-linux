#include "apps/RunningAppModel.h"

RunningAppModel::RunningAppModel(QObject* parent) : QAbstractListModel(parent) {}

int RunningAppModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) {
        return 0;
    }
    return m_entries.size();
}

QVariant RunningAppModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size()) {
        return {};
    }

    const auto& entry = m_entries.at(index.row());
    switch (role) {
        case PidRole:
            return entry.pid;
        case AppNameRole:
            return entry.appName;
        case CommandRole:
            return entry.command;
        default:
            return {};
    }
}

QHash<int, QByteArray> RunningAppModel::roleNames() const {
    return {
        {PidRole, "pid"},
        {AppNameRole, "appName"},
        {CommandRole, "command"},
    };
}

int RunningAppModel::count() const {
    return m_entries.size();
}

void RunningAppModel::addApp(int pid, const QString& appName, const QString& command) {
    beginInsertRows(QModelIndex(), m_entries.size(), m_entries.size());
    m_entries.append({pid, appName, command});
    endInsertRows();
    emit countChanged();
}

void RunningAppModel::removeApp(int pid) {
    for (int i = 0; i < m_entries.size(); ++i) {
        if (m_entries.at(i).pid == pid) {
            beginRemoveRows(QModelIndex(), i, i);
            m_entries.removeAt(i);
            endRemoveRows();
            emit countChanged();
            return;
        }
    }
}

void RunningAppModel::clear() {
    if (m_entries.isEmpty()) {
        return;
    }
    beginResetModel();
    m_entries.clear();
    endResetModel();
    emit countChanged();
}
