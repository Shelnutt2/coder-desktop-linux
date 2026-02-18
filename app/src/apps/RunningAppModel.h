#ifndef RUNNINGAPPMODEL_H
#define RUNNINGAPPMODEL_H

#include <QAbstractListModel>
#include <QString>
#include <QVector>

/// Model exposing running sandboxed apps to QML.
class RunningAppModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Roles {
        PidRole = Qt::UserRole + 1,
        AppNameRole,
        CommandRole,
    };

    explicit RunningAppModel(QObject* parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    [[nodiscard]] int count() const;

    /// Called by DlpCompositorManager when apps start/stop.
    void addApp(int pid, const QString& appName, const QString& command);
    void removeApp(int pid);
    void clear();

signals:
    void countChanged();

private:
    struct Entry {
        int pid;
        QString appName;
        QString command;
    };
    QVector<Entry> m_entries;
};

#endif  // RUNNINGAPPMODEL_H
