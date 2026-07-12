#include "agents/ChatSession.h"

#include <QLoggingCategory>
#include <QSet>
#include <algorithm>

Q_LOGGING_CATEGORY(lcChatSession, "coder.agents.session")

ChatSession::ChatSession(const QString& chatId, QObject* parent)
    : QObject(parent), m_chatId(chatId) {}

// ---------------------------------------------------------------------------
// Durable history
// ---------------------------------------------------------------------------

void ChatSession::sortById(QList<ChatMessage>& messages) {
    std::sort(messages.begin(), messages.end(),
              [](const ChatMessage& a, const ChatMessage& b) { return a.id < b.id; });
}

void ChatSession::setInitialMessages(const QList<ChatMessage>& messages) {
    // The stream socket opens concurrently with the initial REST page, so
    // durable messages applied from stream events can already be present
    // when the page arrives. Merge the page in by id instead of replacing
    // wholesale: page messages win for the ids they cover, and
    // stream-applied messages with other ids are preserved. Reconnect
    // semantics are unaffected because the after_id cursor stays monotonic
    // and history_reset remains the authoritative full-replacement path.
    QList<ChatMessage> merged = messages;
    QSet<qint64> pageIds;
    pageIds.reserve(merged.size());
    for (const ChatMessage& m : merged) pageIds.insert(m.id);
    for (const ChatMessage& m : m_messages) {
        if (!pageIds.contains(m.id)) merged.append(m);
    }
    sortById(merged);
    m_messages = merged;
    if (!m_messages.isEmpty()) m_afterId = qMax(m_afterId, m_messages.last().id);
    emit historyReplaced();
}

void ChatSession::prependOlderMessages(const QList<ChatMessage>& messages) {
    QSet<qint64> known;
    known.reserve(m_messages.size());
    for (const ChatMessage& m : m_messages) known.insert(m.id);

    QList<ChatMessage> older;
    older.reserve(messages.size());
    for (const ChatMessage& m : messages) {
        if (!known.contains(m.id)) older.append(m);
    }
    if (older.isEmpty()) return;
    sortById(older);

    QList<ChatMessage> combined;
    combined.reserve(older.size() + m_messages.size());
    combined.append(older);
    combined.append(m_messages);
    m_messages = combined;
    emit olderMessagesPrepended(static_cast<int>(older.size()));
}

// ---------------------------------------------------------------------------
// Live state
// ---------------------------------------------------------------------------

void ChatSession::setStatus(ChatStatus status) {
    if (m_status == status) return;
    const ChatStatus old = m_status;
    m_status = status;
    emit statusChanged(old, status);
}

void ChatSession::setQueuedMessages(const QList<ChatQueuedMessage>& queued) {
    m_queued = queued;
    emit queueChanged();
}

// ---------------------------------------------------------------------------
// Event application
// ---------------------------------------------------------------------------

void ChatSession::applyEvents(const QList<ChatStreamEvent>& events) {
    for (const ChatStreamEvent& e : events) applyEvent(e);
}

void ChatSession::applyEvent(const ChatStreamEvent& event) {
    // Sub-agent or stale events for another chat are dropped outright.
    if (!event.chatId.isEmpty() && event.chatId != m_chatId) return;

    if (m_historyResetPending) {
        if (event.type == ChatStreamEventType::Message) {
            if (event.hasMessage) m_resetBuffer.append(event.message);
            return;
        }
        // The first non-message event terminates the replacement batch and
        // commits the new history atomically before normal processing.
        commitHistoryReset();
    }

    switch (event.type) {
        case ChatStreamEventType::MessagePart:
            if (event.hasMessagePart) mergeIntoTail(event.messagePart);
            break;
        case ChatStreamEventType::Message:
            if (event.hasMessage) applyMessage(event.message);
            break;
        case ChatStreamEventType::Status:
            if (event.hasStatus) setStatus(event.status);
            break;
        case ChatStreamEventType::Error:
            if (event.hasError) {
                m_lastError = event.error;
                m_hasError = true;
                emit errorChanged();
            }
            break;
        case ChatStreamEventType::QueueUpdate:
            // The event is the authoritative queue snapshot.
            setQueuedMessages(event.queuedMessages);
            break;
        case ChatStreamEventType::Retry:
            if (event.hasRetry) {
                m_retry = event.retry;
                m_hasRetry = true;
                emit retryChanged();
            }
            break;
        case ChatStreamEventType::ActionRequired:
            if (event.hasActionRequired) {
                m_actionRequired = event.actionRequired;
                m_hasActionRequired = true;
                emit actionRequiredChanged();
            }
            break;
        case ChatStreamEventType::PreviewReset:
            if (m_hasTail) {
                m_hasTail = false;
                m_tail = ChatMessage{};
                emit tailChanged();
            }
            break;
        case ChatStreamEventType::HistoryReset:
            m_historyResetPending = true;
            m_resetBuffer.clear();
            break;
        case ChatStreamEventType::Unknown:
            qCDebug(lcChatSession) << "ignoring unknown stream event type" << event.typeString;
            break;
    }
}

void ChatSession::mergeIntoTail(const ChatStreamMessagePart& delta) {
    if (!m_hasTail) {
        m_tail = ChatMessage{};
        m_tail.chatId = m_chatId;
        m_tail.role = delta.role.isEmpty() ? QStringLiteral("assistant") : delta.role;
        m_tail.createdAt = QDateTime::currentDateTimeUtc();
        m_hasTail = true;
    } else if (!delta.role.isEmpty()) {
        m_tail.role = delta.role;
    }
    mergePartInto(m_tail.parts, delta.part);
    emit tailChanged();
}

void ChatSession::mergePartInto(QList<ChatMessagePart>& parts, const ChatMessagePart& incoming) {
    switch (incoming.type) {
        case ChatMessagePartType::Text:
        case ChatMessagePartType::Reasoning: {
            // Streaming text/reasoning deltas accumulate into the single
            // existing part of that type (mirrors the Android ordinal-0
            // coalescing).
            for (ChatMessagePart& p : parts) {
                if (p.type != incoming.type) continue;
                p.text += incoming.text;
                if (!incoming.signature.isEmpty()) p.signature = incoming.signature;
                if (incoming.createdAt.isValid() && !p.createdAt.isValid())
                    p.createdAt = incoming.createdAt;
                if (incoming.completedAt.isValid()) p.completedAt = incoming.completedAt;
                return;
            }
            parts.append(incoming);
            return;
        }
        case ChatMessagePartType::ToolCall: {
            for (ChatMessagePart& p : parts) {
                if (p.type != ChatMessagePartType::ToolCall ||
                    p.toolCallId != incoming.toolCallId) {
                    continue;
                }
                if (!incoming.toolName.isEmpty()) p.toolName = incoming.toolName;
                // action_required redactions arrive with empty args even when
                // the call originally carried real arguments; empty means
                // "no update" so the merged set is preserved.
                if (!incoming.args.isEmpty()) p.args = incoming.args;
                p.argsDelta += incoming.argsDelta;
                p.providerExecuted = p.providerExecuted || incoming.providerExecuted;
                if (incoming.createdAt.isValid() && !p.createdAt.isValid())
                    p.createdAt = incoming.createdAt;
                return;
            }
            parts.append(incoming);
            return;
        }
        case ChatMessagePartType::ToolResult: {
            for (ChatMessagePart& p : parts) {
                if (p.type != ChatMessagePartType::ToolResult ||
                    p.toolCallId != incoming.toolCallId) {
                    continue;
                }
                if (!incoming.toolName.isEmpty()) p.toolName = incoming.toolName;
                if (!incoming.result.isUndefined() && !incoming.result.isNull())
                    p.result = incoming.result;
                // A reset drops the accumulated streaming deltas for this
                // tool call (mirrors coderd message_conversion.go handling
                // of ResultReset) before any new delta is appended.
                if (incoming.resultReset) p.resultDelta.clear();
                p.resultDelta += incoming.resultDelta;
                p.isError = p.isError || incoming.isError;
                if (incoming.createdAt.isValid() && !p.createdAt.isValid())
                    p.createdAt = incoming.createdAt;
                return;
            }
            parts.append(incoming);
            return;
        }
        default:
            parts.append(incoming);
            return;
    }
}

void ChatSession::applyMessage(const ChatMessage& message) {
    // A durable message supersedes any streamed preview: the pending deltas
    // it covers are flushed by clearing the tail.
    const bool hadTail = m_hasTail;
    m_hasTail = false;
    m_tail = ChatMessage{};

    m_afterId = qMax(m_afterId, message.id);

    for (int i = 0; i < m_messages.size(); ++i) {
        if (m_messages.at(i).id == message.id) {
            m_messages[i] = message;
            emit messageUpserted(i, true, hadTail);
            return;
        }
    }
    m_messages.append(message);
    emit messageUpserted(static_cast<int>(m_messages.size()) - 1, false, hadTail);
}

void ChatSession::commitHistoryReset() {
    m_historyResetPending = false;
    QList<ChatMessage> replacement = m_resetBuffer;
    m_resetBuffer.clear();
    sortById(replacement);
    m_messages = replacement;
    m_hasTail = false;
    m_tail = ChatMessage{};
    m_afterId = m_messages.isEmpty() ? 0 : m_messages.last().id;
    emit historyReplaced();
}
