#include "models/ChatMessagesModel.h"

#include <QJsonDocument>
#include <QVariantMap>

namespace {
// Streaming tail deltas coalesce through this timer so per-token events
// repaint the tail row at most ~20 times a second. Raising this from a
// per-frame 16ms materially cuts per-delta QML re-layout work (the tail
// row rebuilds its parts Repeater on every flush).
constexpr int kFlushIntervalMs = 50;
}  // namespace

ChatMessagesModel::ChatMessagesModel(ChatSession* session, QObject* parent)
    : QAbstractListModel(parent), m_session(session) {
    m_flushTimer.setSingleShot(true);
    m_flushTimer.setInterval(kFlushIntervalMs);
    connect(&m_flushTimer, &QTimer::timeout, this, &ChatMessagesModel::flushTail);

    m_durableCount = static_cast<int>(m_session->messages().size());
    m_tailVisible = m_session->hasTail();

    connect(m_session, &ChatSession::tailChanged, this, &ChatMessagesModel::onTailChanged);
    connect(m_session, &ChatSession::messageUpserted, this, &ChatMessagesModel::onMessageUpserted);
    connect(m_session, &ChatSession::historyReplaced, this, &ChatMessagesModel::onHistoryReplaced);
    connect(m_session, &ChatSession::olderMessagesPrepended, this,
            &ChatMessagesModel::onOlderMessagesPrepended);
}

// ---------------------------------------------------------------------------
// QAbstractListModel interface
// ---------------------------------------------------------------------------

int ChatMessagesModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) return 0;
    return m_durableCount + (m_tailVisible ? 1 : 0);
}

int ChatMessagesModel::rowForDurableIndex(int durableIndex) const {
    // Durable messages are stored oldest-first; rows are newest-first with
    // the optional tail row at index 0.
    return (m_durableCount - 1 - durableIndex) + (m_tailVisible ? 1 : 0);
}

QVariant ChatMessagesModel::data(const QModelIndex& index, int role) const {
    const int row = index.row();
    if (row < 0 || row >= rowCount()) return {};

    const bool isTailRow = m_tailVisible && row == 0;
    const int durableIndex = m_durableCount - 1 - (row - (m_tailVisible ? 1 : 0));
    if (!isTailRow && (durableIndex < 0 || durableIndex >= m_session->messages().size())) return {};
    const ChatMessage& msg = isTailRow ? m_session->tail() : m_session->messages().at(durableIndex);

    switch (role) {
        case MessageIdRole:
            return isTailRow ? QVariant(qint64(-1)) : QVariant(msg.id);
        case RoleRole:
            return msg.role;
        case CreatedAtRole:
            return msg.createdAt;
        case IsStreamingRole:
            return isTailRow;
        case PartsRole:
            return partsToVariant(msg);
        default:
            return {};
    }
}

QHash<int, QByteArray> ChatMessagesModel::roleNames() const {
    return {
        {MessageIdRole, "messageId"},     {RoleRole, "role"},   {CreatedAtRole, "createdAt"},
        {IsStreamingRole, "isStreaming"}, {PartsRole, "parts"},
    };
}

QVariantList ChatMessagesModel::partsToVariant(const ChatMessage& message) const {
    // Tool durations pair a tool-result's created_at with the created_at of
    // the matching tool-call.
    QHash<QString, QDateTime> callStarts;
    for (const ChatMessagePart& p : message.parts) {
        if (p.type == ChatMessagePartType::ToolCall && p.createdAt.isValid())
            callStarts.insert(p.toolCallId, p.createdAt);
    }

    QVariantList list;
    list.reserve(message.parts.size());
    for (const ChatMessagePart& p : message.parts) {
        QVariantMap m;
        m.insert(QStringLiteral("type"), p.typeString);
        m.insert(QStringLiteral("text"), p.text);
        m.insert(QStringLiteral("toolCallId"), p.toolCallId);
        m.insert(QStringLiteral("toolName"), p.toolName);

        // Prefer the complete args object; fall back to the accumulated
        // streaming delta text while the call is still arriving.
        QString argsJson;
        if (!p.args.isEmpty()) {
            argsJson = QString::fromUtf8(QJsonDocument(p.args).toJson(QJsonDocument::Compact));
        } else {
            argsJson = p.argsDelta;
        }
        m.insert(QStringLiteral("argsJson"), argsJson);
        m.insert(QStringLiteral("modelIntent"),
                 p.args.value(QLatin1String("model_intent")).toString());

        QString resultText;
        if (p.result.isString()) {
            resultText = p.result.toString();
        } else if (p.result.isObject() || p.result.isArray()) {
            const QJsonDocument doc = p.result.isObject() ? QJsonDocument(p.result.toObject())
                                                          : QJsonDocument(p.result.toArray());
            resultText = QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
        } else if (!p.resultDelta.isEmpty()) {
            resultText = p.resultDelta;
        }
        m.insert(QStringLiteral("resultText"), resultText);
        m.insert(QStringLiteral("isError"), p.isError);

        qint64 durationMs = -1;
        if (p.type == ChatMessagePartType::Reasoning && p.createdAt.isValid() &&
            p.completedAt.isValid()) {
            durationMs = p.createdAt.msecsTo(p.completedAt);
        } else if (p.type == ChatMessagePartType::ToolResult && p.createdAt.isValid()) {
            const QDateTime start = callStarts.value(p.toolCallId);
            if (start.isValid()) durationMs = start.msecsTo(p.createdAt);
        }
        m.insert(QStringLiteral("durationMs"), durationMs);

        m.insert(QStringLiteral("signature"), p.signature);
        m.insert(QStringLiteral("fileId"), p.fileId);
        m.insert(QStringLiteral("url"), p.url);
        m.insert(QStringLiteral("title"), p.title);
        m.insert(QStringLiteral("skillName"), p.skillName);
        list.append(m);
    }
    return list;
}

// ---------------------------------------------------------------------------
// Session change handling
// ---------------------------------------------------------------------------

void ChatMessagesModel::onTailChanged() {
    const bool sessionHasTail = m_session->hasTail();
    if (sessionHasTail && !m_tailVisible) {
        beginInsertRows(QModelIndex(), 0, 0);
        m_tailVisible = true;
        endInsertRows();
        emit countChanged();
        return;
    }
    if (!sessionHasTail && m_tailVisible) {
        removeTailRow();
        return;
    }
    if (sessionHasTail) {
        // Coalesce rapid deltas into one dataChanged per flush interval so
        // the view repaints at most ~60 times a second.
        m_tailDirty = true;
        if (!m_flushTimer.isActive()) m_flushTimer.start();
    }
}

void ChatMessagesModel::flushTail() {
    if (!m_tailDirty || !m_tailVisible) return;
    m_tailDirty = false;
    const QModelIndex idx = index(0);
    // Only the parts payload changes while streaming; the explicit roles
    // vector lets bindings on role/createdAt/isStreaming skip re-evaluation.
    emit dataChanged(idx, idx, {PartsRole});
}

void ChatMessagesModel::removeTailRow() {
    m_flushTimer.stop();
    m_tailDirty = false;
    beginRemoveRows(QModelIndex(), 0, 0);
    m_tailVisible = false;
    endRemoveRows();
    emit countChanged();
}

void ChatMessagesModel::onMessageUpserted(int durableIndex, bool wasExisting, bool hadTail) {
    const bool tailWasVisible = m_tailVisible;
    const bool appendedAtEnd = !wasExisting && durableIndex == m_durableCount;

    if (hadTail && tailWasVisible && appendedAtEnd) {
        // The finalizing message replaces the tail row in place: the row
        // count is unchanged and only row 0 changes content.
        m_flushTimer.stop();
        m_tailDirty = false;
        m_tailVisible = false;
        ++m_durableCount;
        const QModelIndex idx = index(0);
        emit dataChanged(idx, idx,
                         {MessageIdRole, RoleRole, CreatedAtRole, IsStreamingRole, PartsRole});
        return;
    }

    if (hadTail && tailWasVisible) removeTailRow();

    if (wasExisting) {
        const QModelIndex idx = index(rowForDurableIndex(durableIndex));
        // Upserts replace an existing message's content; its id and role
        // are stable.
        emit dataChanged(idx, idx, {CreatedAtRole, PartsRole});
        return;
    }

    // Inserted durable message. Ids ascend oldest-to-newest in the session
    // list, so the new row index mirrors the durable position: an append at
    // the newest end lands at row 0 (after the optional tail row), an
    // out-of-order stream replay lands further down.
    const int row = (m_durableCount - durableIndex) + (m_tailVisible ? 1 : 0);
    beginInsertRows(QModelIndex(), row, row);
    ++m_durableCount;
    endInsertRows();
    emit countChanged();
}

void ChatMessagesModel::onHistoryReplaced() {
    // Initial page loads and explicit history_reset events replace the whole
    // history; a reset is correct here because the content is discontinuous.
    beginResetModel();
    m_flushTimer.stop();
    m_tailDirty = false;
    m_durableCount = static_cast<int>(m_session->messages().size());
    m_tailVisible = m_session->hasTail();
    endResetModel();
    emit countChanged();
}

void ChatMessagesModel::onOlderMessagesPrepended(int count) {
    // Older messages sit at the front of the durable list, which maps to the
    // highest row indices (oldest end) in this newest-first model.
    const int first = rowCount();
    beginInsertRows(QModelIndex(), first, first + count - 1);
    m_durableCount += count;
    endInsertRows();
    emit countChanged();
}

// ---------------------------------------------------------------------------
// Pagination
// ---------------------------------------------------------------------------

void ChatMessagesModel::setHasMore(bool hasMore) {
    if (m_hasMore == hasMore) return;
    m_hasMore = hasMore;
    emit hasMoreChanged();
}

void ChatMessagesModel::loadOlder() {
    if (!m_hasMore || m_loadingOlder) return;
    if (m_session->messages().isEmpty()) return;
    m_loadingOlder = true;
    emit loadingOlderChanged();
    emit loadOlderRequested(m_session->messages().first().id);
}

void ChatMessagesModel::finishLoadOlder(bool hasMore) {
    setHasMore(hasMore);
    if (!m_loadingOlder) return;
    m_loadingOlder = false;
    emit loadingOlderChanged();
}

// ---------------------------------------------------------------------------
// Row queries
// ---------------------------------------------------------------------------

bool ChatMessagesModel::hasNewerUserMessage(int row) const {
    // Rows are newest-first; anything below `row` (smaller indices) is newer.
    const int count = rowCount();
    if (row <= 0 || row > count) return false;
    for (int r = 0; r < row && r < count; ++r) {
        const bool isTailRow = m_tailVisible && r == 0;
        const int durableIndex = m_durableCount - 1 - (r - (m_tailVisible ? 1 : 0));
        if (!isTailRow && (durableIndex < 0 || durableIndex >= m_session->messages().size()))
            continue;
        const ChatMessage& msg =
            isTailRow ? m_session->tail() : m_session->messages().at(durableIndex);
        if (msg.role == QLatin1String("user")) return true;
    }
    return false;
}
