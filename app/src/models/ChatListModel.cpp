#include "models/ChatListModel.h"

#include <QDate>
#include <QDateTime>
#include <algorithm>

ChatListModel::ChatListModel(QObject* parent) : QAbstractListModel(parent) {}

// ---------------------------------------------------------------------------
// QAbstractListModel interface
// ---------------------------------------------------------------------------

int ChatListModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) return 0;
    return static_cast<int>(m_rows.size());
}

QVariant ChatListModel::data(const QModelIndex& index, int role) const {
    const int row = index.row();
    if (row < 0 || row >= m_rows.size()) return {};
    const Row& r = m_rows.at(row);
    const Chat& c = r.chat;

    switch (role) {
        case IdRole:
            return c.id;
        case TitleRole:
            return c.title;
        case StatusRole:
            return static_cast<int>(c.status);
        case StatusStringRole:
            return c.statusString;
        case HasUnreadRole:
            return c.hasUnread;
        case PinnedRole:
            return c.isPinned();
        case PinOrderRole:
            return c.pinOrder;
        case ArchivedRole:
            return c.archived;
        case WorkspaceIdRole:
            return c.workspaceId;
        case WorkspaceNameRole:
            return m_workspaceNames.value(c.workspaceId);
        case UpdatedAtRole:
            return c.updatedAt;
        case RelativeTimeRole:
            return relativeTimeFor(c.updatedAt);
        case TimeGroupRole:
            // Sub-agents inherit their position (and section) from the
            // parent row above them; grouping by the child's own timestamp
            // would split the parent's section with interleaved headers.
            if (r.isSubagent) {
                for (const Chat& parent : m_chats) {
                    if (parent.id == r.parentId) return timeGroupFor(parent);
                }
            }
            return timeGroupFor(c);
        case DiffStatusRole:
            return c.diffStatus.hasValue ? QStringLiteral("+%1 -%2")
                                               .arg(c.diffStatus.additions)
                                               .arg(c.diffStatus.deletions)
                                         : QString();
        case IsSubagentRole:
            return r.isSubagent;
        case ParentIdRole:
            return r.parentId;
        case RequiresActionRole:
            return c.status == ChatStatus::RequiresAction;
        case ChildCountRole:
            return static_cast<int>(c.children.size());
        default:
            return {};
    }
}

QHash<int, QByteArray> ChatListModel::roleNames() const {
    return {
        {IdRole, "id"},
        {TitleRole, "title"},
        {StatusRole, "status"},
        {StatusStringRole, "statusString"},
        {HasUnreadRole, "hasUnread"},
        {PinnedRole, "pinned"},
        {PinOrderRole, "pinOrder"},
        {ArchivedRole, "archived"},
        {WorkspaceIdRole, "workspaceId"},
        {WorkspaceNameRole, "workspaceName"},
        {UpdatedAtRole, "updatedAt"},
        {RelativeTimeRole, "relativeTime"},
        {TimeGroupRole, "timeGroup"},
        {DiffStatusRole, "diffStatus"},
        {IsSubagentRole, "isSubagent"},
        {ParentIdRole, "parentId"},
        {RequiresActionRole, "requiresAction"},
        {ChildCountRole, "childCount"},
    };
}

// ---------------------------------------------------------------------------
// Data input
// ---------------------------------------------------------------------------

void ChatListModel::setChats(const QList<Chat>& chats) {
    m_chats = chats;
    // Granular sync keeps the view's scroll position and selection intact
    // when the 15s polling fallback refreshes the list; a model reset
    // would yank both.
    syncRows(buildRows());
}

void ChatListModel::upsertChat(const Chat& chat) {
    if (chat.id.isEmpty()) return;

    if (chat.isSubagent()) {
        // Sub-agent chats live inside their parent's children.
        for (Chat& parent : m_chats) {
            if (parent.id != chat.parentChatId) continue;
            bool replaced = false;
            for (Chat& child : parent.children) {
                if (child.id == chat.id) {
                    child = chat;
                    replaced = true;
                    break;
                }
            }
            if (!replaced) parent.children.append(chat);
            break;
        }
    } else {
        bool replaced = false;
        for (Chat& existing : m_chats) {
            if (existing.id != chat.id) continue;
            // Watch payloads are lightweight and may omit children; keep the
            // known children in that case.
            Chat merged = chat;
            if (merged.children.isEmpty()) merged.children = existing.children;
            existing = merged;
            replaced = true;
            break;
        }
        if (!replaced) m_chats.append(chat);
    }

    syncRows(buildRows());
}

void ChatListModel::removeChat(const QString& chatId) {
    bool changed = false;
    for (int i = 0; i < m_chats.size(); ++i) {
        if (m_chats.at(i).id == chatId) {
            m_chats.removeAt(i);
            changed = true;
            break;
        }
        Chat& parent = m_chats[i];
        for (int j = 0; j < parent.children.size(); ++j) {
            if (parent.children.at(j).id == chatId) {
                parent.children.removeAt(j);
                changed = true;
                break;
            }
        }
        if (changed) break;
    }
    if (changed) syncRows(buildRows());
}

void ChatListModel::setWorkspaceNames(const QHash<QString, QString>& names) {
    m_workspaceNames = names;
    if (!m_rows.isEmpty())
        emit dataChanged(index(0), index(static_cast<int>(m_rows.size()) - 1), {WorkspaceNameRole});
}

// ---------------------------------------------------------------------------
// Filtering
// ---------------------------------------------------------------------------

void ChatListModel::setSearchText(const QString& text) {
    if (m_searchText == text) return;
    m_searchText = text;
    emit searchTextChanged();
    syncRows(buildRows());
}

void ChatListModel::setFilter(Filter filter) {
    if (m_filter == filter) return;
    m_filter = filter;
    emit filterChanged();
    syncRows(buildRows());
}

bool ChatListModel::matchesFilter(const Chat& chat) const {
    switch (m_filter) {
        case Filter::All:
            return !chat.archived;
        case Filter::Unread:
            return !chat.archived && chat.hasUnread;
        case Filter::RequiresAction:
            return !chat.archived && chat.status == ChatStatus::RequiresAction;
        case Filter::Archived:
            return chat.archived;
    }
    return true;
}

bool ChatListModel::matchesSearch(const Chat& chat) const {
    if (m_searchText.isEmpty()) return true;
    if (chat.title.contains(m_searchText, Qt::CaseInsensitive)) return true;
    for (const Chat& child : chat.children) {
        if (child.title.contains(m_searchText, Qt::CaseInsensitive)) return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// Row building and granular synchronisation
// ---------------------------------------------------------------------------

QList<ChatListModel::Row> ChatListModel::buildRows() const {
    QList<Chat> visible;
    visible.reserve(m_chats.size());
    for (const Chat& c : m_chats) {
        if (matchesFilter(c) && matchesSearch(c)) visible.append(c);
    }

    std::sort(visible.begin(), visible.end(), [](const Chat& a, const Chat& b) {
        // Pinned chats first, ordered by ascending pin position; the rest by
        // most recent activity.
        if (a.isPinned() != b.isPinned()) return a.isPinned();
        if (a.isPinned() && b.isPinned() && a.pinOrder != b.pinOrder)
            return a.pinOrder < b.pinOrder;
        if (a.updatedAt != b.updatedAt) return a.updatedAt > b.updatedAt;
        return a.id < b.id;
    });

    QList<Row> rows;
    rows.reserve(visible.size() * 2);
    for (const Chat& c : visible) {
        rows.append(Row{c, false, QString()});
        // Sub-agents appear immediately after their parent, newest first.
        QList<Chat> children = c.children;
        std::sort(children.begin(), children.end(), [](const Chat& a, const Chat& b) {
            if (a.updatedAt != b.updatedAt) return a.updatedAt > b.updatedAt;
            return a.id < b.id;
        });
        for (const Chat& child : children) rows.append(Row{child, true, c.id});
    }
    return rows;
}

bool ChatListModel::sameDisplay(const Row& a, const Row& b) {
    return a.chat.title == b.chat.title && a.chat.status == b.chat.status &&
           a.chat.hasUnread == b.chat.hasUnread && a.chat.pinOrder == b.chat.pinOrder &&
           a.chat.archived == b.chat.archived && a.chat.updatedAt == b.chat.updatedAt &&
           a.chat.workspaceId == b.chat.workspaceId &&
           a.chat.diffStatus.additions == b.chat.diffStatus.additions &&
           a.chat.diffStatus.deletions == b.chat.diffStatus.deletions &&
           a.chat.children.size() == b.chat.children.size() && a.isSubagent == b.isSubagent &&
           a.parentId == b.parentId;
}

void ChatListModel::syncRows(const QList<Row>& target) {
    const bool countWillChange = m_rows.size() != target.size();

    // Walk the target order; at each position either the row already
    // matches, can be moved up from a later position, or must be inserted.
    for (int i = 0; i < target.size(); ++i) {
        const Row& want = target.at(i);
        if (i < m_rows.size() && m_rows.at(i).chat.id == want.chat.id &&
            m_rows.at(i).isSubagent == want.isSubagent) {
            if (!sameDisplay(m_rows.at(i), want)) {
                m_rows[i] = want;
                emit dataChanged(index(i), index(i));
            } else {
                m_rows[i] = want;
            }
            continue;
        }
        int found = -1;
        for (int j = i + 1; j < m_rows.size(); ++j) {
            if (m_rows.at(j).chat.id == want.chat.id &&
                m_rows.at(j).isSubagent == want.isSubagent) {
                found = j;
                break;
            }
        }
        if (found >= 0) {
            beginMoveRows(QModelIndex(), found, found, QModelIndex(), i);
            m_rows.move(found, i);
            endMoveRows();
            if (!sameDisplay(m_rows.at(i), want)) {
                m_rows[i] = want;
                emit dataChanged(index(i), index(i));
            } else {
                m_rows[i] = want;
            }
        } else {
            beginInsertRows(QModelIndex(), i, i);
            m_rows.insert(i, want);
            endInsertRows();
        }
    }

    // Rows past the target length no longer belong in the view.
    while (m_rows.size() > target.size()) {
        const int last = static_cast<int>(m_rows.size()) - 1;
        beginRemoveRows(QModelIndex(), last, last);
        m_rows.removeAt(last);
        endRemoveRows();
    }

    if (countWillChange) emit countChanged();
}

// ---------------------------------------------------------------------------
// Time helpers
// ---------------------------------------------------------------------------

QString ChatListModel::timeGroupFor(const Chat& chat) {
    if (chat.isPinned()) return QStringLiteral("Pinned");
    const QDateTime now = QDateTime::currentDateTime();
    const QDate today = now.date();
    const QDate d = chat.updatedAt.toLocalTime().date();
    if (!d.isValid()) return QStringLiteral("Older");
    if (d == today) return QStringLiteral("Today");
    if (d == today.addDays(-1)) return QStringLiteral("Yesterday");
    if (d >= today.addDays(-7)) return QStringLiteral("This Week");
    if (d.year() == today.year() && d.month() == today.month()) return QStringLiteral("This Month");
    return QStringLiteral("Older");
}

QString ChatListModel::relativeTimeFor(const QDateTime& dt) {
    if (!dt.isValid()) return {};
    const qint64 secs = dt.secsTo(QDateTime::currentDateTimeUtc());
    if (secs < 60) return QStringLiteral("now");
    if (secs < 3600) return QStringLiteral("%1m").arg(secs / 60);
    if (secs < 86400) return QStringLiteral("%1h").arg(secs / 3600);
    if (secs < 7 * 86400) return QStringLiteral("%1d").arg(secs / 86400);
    return dt.toLocalTime().date().toString(QStringLiteral("MMM d"));
}
