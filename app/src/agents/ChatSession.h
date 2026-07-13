#ifndef CHATSESSION_H
#define CHATSESSION_H

#include <QList>
#include <QObject>
#include <QString>

#include "api/dto/Chat.h"
#include "api/dto/ChatMessage.h"
#include "api/dto/ChatStreamEvent.h"

/// UI-free merge engine for one open chat.
///
/// Owns the durable message list, the in-memory streaming tail (the preview
/// message accumulated from message_part deltas and never persisted), the
/// chat status, the queued-message snapshot, the retry state, the
/// action-required state, and the last stream error.
///
/// applyEvent() implements the delta-merge rules shared with the Android
/// client and the web UI:
/// - text/reasoning deltas append onto the existing part of that type;
/// - tool-call/tool-result parts merge by tool_call_id, concatenating
///   args_delta/result_delta; empty incoming args mean "no update";
/// - a message event flushes the tail, upserts the durable message, and
///   advances the after_id resume cursor;
/// - queue_update is the authoritative queue snapshot;
/// - preview_reset drops the tail;
/// - history_reset buffers subsequent message events until the next
///   non-message event, then atomically replaces the durable history;
/// - events whose chat_id does not match this session are dropped.
///
/// Signals are granular so a list model can update rows without resets.
class ChatSession : public QObject {
    Q_OBJECT

public:
    explicit ChatSession(const QString& chatId, QObject* parent = nullptr);

    [[nodiscard]] QString chatId() const { return m_chatId; }

    // -- Durable history -------------------------------------------------------
    [[nodiscard]] const QList<ChatMessage>& messages() const { return m_messages; }
    /// Merges the initial REST page into the durable history by id: page
    /// messages win for the ids they cover; messages already applied from
    /// concurrent stream events with other ids are preserved. Messages are
    /// sorted ascending by id regardless of input order.
    void setInitialMessages(const QList<ChatMessage>& messages);
    /// Inserts an older page before the current history. Messages are sorted
    /// ascending by id; duplicates of already-known ids are dropped.
    void prependOlderMessages(const QList<ChatMessage>& messages);

    // -- Streaming tail --------------------------------------------------------
    [[nodiscard]] bool hasTail() const { return m_hasTail; }
    [[nodiscard]] const ChatMessage& tail() const { return m_tail; }

    // -- Live state ------------------------------------------------------------
    [[nodiscard]] ChatStatus status() const { return m_status; }
    void setStatus(ChatStatus status);
    [[nodiscard]] const QList<ChatQueuedMessage>& queuedMessages() const { return m_queued; }
    void setQueuedMessages(const QList<ChatQueuedMessage>& queued);
    [[nodiscard]] bool hasRetry() const { return m_hasRetry; }
    [[nodiscard]] const ChatStreamRetry& retry() const { return m_retry; }
    [[nodiscard]] bool hasActionRequired() const { return m_hasActionRequired; }
    [[nodiscard]] const ChatStreamActionRequired& actionRequired() const {
        return m_actionRequired;
    }
    [[nodiscard]] bool hasError() const { return m_hasError; }
    [[nodiscard]] const ChatError& lastError() const { return m_lastError; }

    /// Resume cursor: the highest durable message id seen so far.
    [[nodiscard]] qint64 afterId() const { return m_afterId; }

    /// Applies one stream event per the delta-merge rules.
    void applyEvent(const ChatStreamEvent& event);
    void applyEvents(const QList<ChatStreamEvent>& events);

signals:
    /// The streaming tail changed (delta merged, tail appeared or cleared).
    void tailChanged();
    /// A durable message changed. If wasExisting, the message at index was
    /// replaced in place; otherwise it was appended at index (the end).
    /// hadTail reports whether the streaming tail was cleared by this event.
    void messageUpserted(int index, bool wasExisting, bool hadTail);
    /// The whole durable history was atomically replaced (history_reset).
    void historyReplaced();
    /// count older messages were inserted at the front of the durable list.
    void olderMessagesPrepended(int count);
    void statusChanged(ChatStatus oldStatus, ChatStatus newStatus);
    void queueChanged();
    void retryChanged();
    void actionRequiredChanged();
    void errorChanged();

private:
    void mergeIntoTail(const ChatStreamMessagePart& delta);
    void applyMessage(const ChatMessage& message);
    void commitHistoryReset();
    static void mergePartInto(QList<ChatMessagePart>& parts, const ChatMessagePart& incoming);
    static void sortById(QList<ChatMessage>& messages);

    QString m_chatId;
    QList<ChatMessage> m_messages;  // ascending by id
    ChatMessage m_tail;
    bool m_hasTail = false;
    ChatStatus m_status = ChatStatus::Unknown;
    QList<ChatQueuedMessage> m_queued;
    ChatStreamRetry m_retry;
    bool m_hasRetry = false;
    ChatStreamActionRequired m_actionRequired;
    bool m_hasActionRequired = false;
    ChatError m_lastError;
    bool m_hasError = false;
    qint64 m_afterId = 0;

    // history_reset buffering: replacement messages accumulate here until
    // the next non-message event commits them atomically.
    bool m_historyResetPending = false;
    QList<ChatMessage> m_resetBuffer;
};

#endif  // CHATSESSION_H
