#ifndef CHATLISTMODEL_H
#define CHATLISTMODEL_H

#include <QAbstractListModel>
#include <QHash>
#include <QList>
#include <QString>

#include "api/dto/Chat.h"

/// List model exposing the Coder Agents chat list to QML.
///
/// Rows are top-level chats sorted pinned-first (pin_order ascending) then
/// updatedAt descending, with each chat's sub-agent children placed
/// immediately after their parent. Client-side search and filtering are
/// applied before sorting.
///
/// setChats() replaces the whole list (initial load / poll refresh).
/// setChats(), upsertChat(), and removeChat() all apply changes with
/// granular insert/remove/move/dataChanged operations, never model resets,
/// so list views keep their scroll position and selection.
class ChatListModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    Q_PROPERTY(QString searchText READ searchText WRITE setSearchText NOTIFY searchTextChanged)
    Q_PROPERTY(Filter filter READ filter WRITE setFilter NOTIFY filterChanged)

public:
    enum Roles {
        IdRole = Qt::UserRole + 1,
        TitleRole,
        StatusRole,
        StatusStringRole,
        HasUnreadRole,
        PinnedRole,
        PinOrderRole,
        ArchivedRole,
        WorkspaceIdRole,
        WorkspaceNameRole,
        UpdatedAtRole,
        RelativeTimeRole,
        TimeGroupRole,
        DiffStatusRole,
        IsSubagentRole,
        ParentIdRole,
        RequiresActionRole,
        ChildCountRole,
    };
    Q_ENUM(Roles)

    enum class Filter {
        All,
        Unread,
        RequiresAction,
        Archived,
    };
    Q_ENUM(Filter)

    explicit ChatListModel(QObject* parent = nullptr);

    // -- QAbstractListModel interface --
    [[nodiscard]] int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    // -- Data input --
    /// Replaces the full chat list (initial load or poll refresh).
    void setChats(const QList<Chat>& chats);
    /// Applies a created/updated watch event granularly. Sub-agent chats are
    /// merged into their parent's children. When the incoming payload has no
    /// children (watch payloads are lightweight), existing children are kept.
    void upsertChat(const Chat& chat);
    /// Applies a deleted watch event granularly.
    void removeChat(const QString& chatId);
    [[nodiscard]] const QList<Chat>& chats() const { return m_chats; }

    /// Resolves workspaceName roles; keys are workspace ids.
    void setWorkspaceNames(const QHash<QString, QString>& names);

    // -- Filtering --
    [[nodiscard]] QString searchText() const { return m_searchText; }
    void setSearchText(const QString& text);
    [[nodiscard]] Filter filter() const { return m_filter; }
    void setFilter(Filter filter);

signals:
    void countChanged();
    void searchTextChanged();
    void filterChanged();

private:
    struct Row {
        Chat chat;
        bool isSubagent = false;
        QString parentId;
    };

    [[nodiscard]] QList<Row> buildRows() const;
    [[nodiscard]] bool matchesFilter(const Chat& chat) const;
    [[nodiscard]] bool matchesSearch(const Chat& chat) const;
    /// Transforms the current visible rows into target using granular
    /// remove/move/insert/dataChanged operations.
    void syncRows(const QList<Row>& target);
    [[nodiscard]] static bool sameDisplay(const Row& a, const Row& b);
    [[nodiscard]] static QString timeGroupFor(const Chat& chat);
    [[nodiscard]] static QString relativeTimeFor(const QDateTime& dt);

    QList<Chat> m_chats;  // top-level chats with nested children
    QList<Row> m_rows;    // visible rows after filter/sort/flatten
    QHash<QString, QString> m_workspaceNames;
    QString m_searchText;
    Filter m_filter = Filter::All;
};

#endif  // CHATLISTMODEL_H
