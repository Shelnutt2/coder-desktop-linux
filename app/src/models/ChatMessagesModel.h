#ifndef CHATMESSAGESMODEL_H
#define CHATMESSAGESMODEL_H

#include <QAbstractListModel>
#include <QTimer>
#include <QVariantList>

#include "agents/ChatSession.h"

/// List model exposing one ChatSession's messages to a QML ListView with
/// verticalLayoutDirection: BottomToTop.
///
/// Row order: index 0 is the NEWEST row. While a response is streaming, a
/// single streaming-tail row occupies index 0; durable messages follow in
/// newest-to-oldest order. Older pagination pages append at the END indices
/// (highest index = oldest message), which keeps the viewport stable in a
/// BottomToTop view.
///
/// Scroll stability rules:
/// - Streaming deltas never reset the model. Tail updates coalesce through a
///   ~16ms timer and flush as a single dataChanged on the tail row.
/// - When a durable message finalizes the current tail, the tail row is
///   converted in place with dataChanged (row count is unchanged) whenever
///   the message appends at the newest position.
class ChatMessagesModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    Q_PROPERTY(bool hasMore READ hasMore NOTIFY hasMoreChanged)
    Q_PROPERTY(bool loadingOlder READ isLoadingOlder NOTIFY loadingOlderChanged)

public:
    enum Roles {
        MessageIdRole = Qt::UserRole + 1,
        RoleRole,
        CreatedAtRole,
        IsStreamingRole,
        PartsRole,
    };
    Q_ENUM(Roles)

    /// session must outlive this model (typically both share a parent).
    explicit ChatMessagesModel(ChatSession* session, QObject* parent = nullptr);

    // -- QAbstractListModel interface --
    [[nodiscard]] int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    // -- Pagination --
    [[nodiscard]] bool hasMore() const { return m_hasMore; }
    void setHasMore(bool hasMore);
    [[nodiscard]] bool isLoadingOlder() const { return m_loadingOlder; }
    /// Requests the next older page when hasMore and no request is in
    /// flight. Emits loadOlderRequested() with the oldest known message id.
    Q_INVOKABLE void loadOlder();
    /// Completes a loadOlder() cycle (call after prependOlderMessages or on
    /// fetch failure).
    void finishLoadOlder(bool hasMore);

signals:
    void countChanged();
    void hasMoreChanged();
    void loadingOlderChanged();
    /// Emitted by loadOlder(); the owner fetches messages?before_id=beforeId.
    void loadOlderRequested(qint64 beforeId);

private:
    void onTailChanged();
    void onMessageUpserted(int index, bool wasExisting, bool hadTail);
    void onHistoryReplaced();
    void onOlderMessagesPrepended(int count);
    void flushTail();
    void removeTailRow();

    [[nodiscard]] int rowForDurableIndex(int durableIndex) const;
    [[nodiscard]] QVariantList partsToVariant(const ChatMessage& message) const;

    ChatSession* m_session = nullptr;  // non-owning
    // Cached counts so rowCount() stays consistent with begin/end bracketing
    // even though the session mutates its lists before emitting signals.
    int m_durableCount = 0;
    bool m_tailVisible = false;
    bool m_tailDirty = false;
    QTimer m_flushTimer;
    bool m_hasMore = false;
    bool m_loadingOlder = false;
};

#endif  // CHATMESSAGESMODEL_H
