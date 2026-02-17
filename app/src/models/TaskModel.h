#ifndef TASKMODEL_H
#define TASKMODEL_H

#include <QAbstractListModel>
#include <QDateTime>
#include <QJsonObject>
#include <QString>

#include "api/dto/Task.h"

/// List model exposing Coder AI tasks to QML views.
///
/// Data is pushed from C++ via setTasks() / updateTask() / clear().
/// QML binds to the roles defined in Roles and the count / loading / error
/// properties.
class TaskModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    Q_PROPERTY(bool loading READ isLoading NOTIFY loadingChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorChanged)

public:
    enum Roles {
        IdRole = Qt::UserRole + 1,
        NameRole,
        DisplayNameRole,
        OwnerNameRole,
        OwnerAvatarRole,
        TemplateNameRole,
        TemplateIconRole,
        WorkspaceNameRole,
        InitialPromptRole,
        StatusRole,
        StatusStringRole,
        CurrentStateRole,
        CurrentStateMessageRole,
        CreatedAtRole,
        UpdatedAtRole,
    };
    Q_ENUM(Roles)

    /// Simplified task data transferred into the model.
    struct TaskInfo {
        QString id;
        QString name;
        QString displayName;
        QString ownerName;
        QString ownerAvatarUrl;
        QString templateName;
        QString templateIcon;
        QString workspaceName;
        QString initialPrompt;
        int status = static_cast<int>(TaskStatus::Unknown);
        QString currentStateMessage;
        int currentState = static_cast<int>(TaskState::Idle);
        QDateTime createdAt;
        QDateTime updatedAt;

        /// Parse a Coder task JSON object into TaskInfo.
        static TaskInfo fromJson(const QJsonObject& obj);

        /// Convert from a full Task DTO.
        static TaskInfo fromTask(const Task& task);
    };

    explicit TaskModel(QObject* parent = nullptr);

    // -- QAbstractListModel interface --
    [[nodiscard]] int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    // -- Bulk / incremental updates (called from C++) --
    void setTasks(const QList<TaskInfo>& tasks);
    void updateTask(const TaskInfo& task);
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
    QList<TaskInfo> m_tasks;
    bool m_loading = false;
    QString m_errorMessage;
};

#endif  // TASKMODEL_H
