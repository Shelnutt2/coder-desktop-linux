#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTest>

#include "agents/ChatSession.h"

/// Unit tests for the ChatSession delta-merge engine: table-driven stream
/// event sequences covering deltas, tool merging, finalization, resets,
/// queue snapshots, cross-chat drops, status transitions, and the after_id
/// cursor.
class TestChatSession : public QObject {
    Q_OBJECT

    static ChatStreamEvent eventFromJson(const char* json) {
        return ChatStreamEvent::fromJson(QJsonDocument::fromJson(json).object());
    }

    static ChatStreamEvent textDelta(const QString& chatId, const QString& text) {
        QJsonObject part{{"type", "text"}, {"text", text}};
        QJsonObject mp{{"role", "assistant"}, {"part", part}};
        return ChatStreamEvent::fromJson(
            QJsonObject{{"type", "message_part"}, {"chat_id", chatId}, {"message_part", mp}});
    }

    static ChatStreamEvent messageEvent(const QString& chatId, qint64 id, const QString& text) {
        QJsonObject part{{"type", "text"}, {"text", text}};
        QJsonObject msg{{"id", double(id)},
                        {"chat_id", chatId},
                        {"role", "assistant"},
                        {"content", QJsonArray{part}}};
        return ChatStreamEvent::fromJson(
            QJsonObject{{"type", "message"}, {"chat_id", chatId}, {"message", msg}});
    }

    static ChatStreamEvent statusEvent(const QString& chatId, const QString& status) {
        return ChatStreamEvent::fromJson(QJsonObject{
            {"type", "status"}, {"chat_id", chatId}, {"status", QJsonObject{{"status", status}}}});
    }

private slots:
    void initTestCase() { qRegisterMetaType<ChatStatus>(); }

    // -----------------------------------------------------------------------
    // Text / reasoning delta appends
    // -----------------------------------------------------------------------

    void testTextDeltaAppend() {
        ChatSession s(QStringLiteral("c1"));
        QSignalSpy tailSpy(&s, &ChatSession::tailChanged);

        s.applyEvent(textDelta(QStringLiteral("c1"), QStringLiteral("Hel")));
        s.applyEvent(textDelta(QStringLiteral("c1"), QStringLiteral("lo")));

        QVERIFY(s.hasTail());
        QCOMPARE(s.tail().parts.size(), 1);
        QCOMPARE(s.tail().parts.first().text, QStringLiteral("Hello"));
        QCOMPARE(s.tail().role, QStringLiteral("assistant"));
        QCOMPARE(tailSpy.count(), 2);
    }

    void testReasoningDeltaAppendAndSignature() {
        ChatSession s(QStringLiteral("c1"));
        s.applyEvent(eventFromJson(
            R"({"type": "message_part", "chat_id": "c1",
                "message_part": {"role": "assistant",
                                 "part": {"type": "reasoning", "text": "thin"}}})"));
        s.applyEvent(eventFromJson(
            R"({"type": "message_part", "chat_id": "c1",
                "message_part": {"role": "assistant",
                                 "part": {"type": "reasoning", "text": "king",
                                          "signature": "sig1"}}})"));
        QCOMPARE(s.tail().parts.size(), 1);
        QCOMPARE(s.tail().parts.first().text, QStringLiteral("thinking"));
        QCOMPARE(s.tail().parts.first().signature, QStringLiteral("sig1"));
    }

    void testTextAndReasoningStaySeparate() {
        ChatSession s(QStringLiteral("c1"));
        s.applyEvent(eventFromJson(
            R"({"type": "message_part", "chat_id": "c1",
                "message_part": {"part": {"type": "reasoning", "text": "hmm"}}})"));
        s.applyEvent(textDelta(QStringLiteral("c1"), QStringLiteral("answer")));
        QCOMPARE(s.tail().parts.size(), 2);
        QCOMPARE(s.tail().parts[0].typeString, QStringLiteral("reasoning"));
        QCOMPARE(s.tail().parts[1].typeString, QStringLiteral("text"));
    }

    // -----------------------------------------------------------------------
    // Tool-call / tool-result merging by tool_call_id
    // -----------------------------------------------------------------------

    void testToolCallMergeById() {
        ChatSession s(QStringLiteral("c1"));
        s.applyEvent(eventFromJson(
            R"({"type": "message_part", "chat_id": "c1",
                "message_part": {"part": {"type": "tool-call", "tool_call_id": "tc1",
                                          "tool_name": "execute",
                                          "args_delta": "{'cmd':"}}})"));
        s.applyEvent(eventFromJson(
            R"({"type": "message_part", "chat_id": "c1",
                "message_part": {"part": {"type": "tool-call", "tool_call_id": "tc1",
                                          "args_delta": "'ls'}"}}})"));
        QCOMPARE(s.tail().parts.size(), 1);
        const ChatMessagePart& p = s.tail().parts.first();
        QCOMPARE(p.toolName, QStringLiteral("execute"));
        QCOMPARE(p.argsDelta, QStringLiteral("{'cmd':'ls'}"));
    }

    void testToolCallEmptyArgsIsNoUpdate() {
        ChatSession s(QStringLiteral("c1"));
        s.applyEvent(eventFromJson(
            R"({"type": "message_part", "chat_id": "c1",
                "message_part": {"part": {"type": "tool-call", "tool_call_id": "tc1",
                                          "tool_name": "execute",
                                          "args": {"cmd": "ls"}}}})"));
        // action_required redactions send empty args; they must not clobber.
        s.applyEvent(eventFromJson(
            R"({"type": "message_part", "chat_id": "c1",
                "message_part": {"part": {"type": "tool-call", "tool_call_id": "tc1",
                                          "args": {}}}})"));
        QCOMPARE(s.tail().parts.size(), 1);
        QCOMPARE(s.tail().parts.first().args.value("cmd").toString(), QStringLiteral("ls"));
    }

    void testToolResultMergeById() {
        ChatSession s(QStringLiteral("c1"));
        s.applyEvent(eventFromJson(
            R"({"type": "message_part", "chat_id": "c1",
                "message_part": {"part": {"type": "tool-result", "tool_call_id": "tc1",
                                          "result_delta": "line1\n"}}})"));
        s.applyEvent(eventFromJson(
            R"({"type": "message_part", "chat_id": "c1",
                "message_part": {"part": {"type": "tool-result", "tool_call_id": "tc1",
                                          "result_delta": "line2", "is_error": true}}})"));
        QCOMPARE(s.tail().parts.size(), 1);
        QCOMPARE(s.tail().parts.first().resultDelta, QStringLiteral("line1\nline2"));
        QVERIFY(s.tail().parts.first().isError);
    }

    void testToolResultResetClearsAccumulatedDeltas() {
        ChatSession s(QStringLiteral("c1"));
        s.applyEvent(eventFromJson(
            R"({"type": "message_part", "chat_id": "c1",
                "message_part": {"part": {"type": "tool-result", "tool_call_id": "tc1",
                                          "result_delta": "stale1"}}})"));
        s.applyEvent(eventFromJson(
            R"({"type": "message_part", "chat_id": "c1",
                "message_part": {"part": {"type": "tool-result", "tool_call_id": "tc1",
                                          "result_delta": "stale2"}}})"));
        // result_reset drops the accumulated deltas (mirrors coderd
        // message_conversion.go ResultReset handling).
        s.applyEvent(eventFromJson(
            R"({"type": "message_part", "chat_id": "c1",
                "message_part": {"part": {"type": "tool-result", "tool_call_id": "tc1",
                                          "result_reset": true}}})"));
        QCOMPARE(s.tail().parts.size(), 1);
        QCOMPARE(s.tail().parts.first().resultDelta, QString());
        // Deltas after the reset accumulate from scratch.
        s.applyEvent(eventFromJson(
            R"({"type": "message_part", "chat_id": "c1",
                "message_part": {"part": {"type": "tool-result", "tool_call_id": "tc1",
                                          "result_delta": "fresh"}}})"));
        QCOMPARE(s.tail().parts.first().resultDelta, QStringLiteral("fresh"));
    }

    void testDistinctToolCallIdsStaySeparate() {
        ChatSession s(QStringLiteral("c1"));
        s.applyEvent(eventFromJson(
            R"({"type": "message_part", "chat_id": "c1",
                "message_part": {"part": {"type": "tool-call", "tool_call_id": "tc1"}}})"));
        s.applyEvent(eventFromJson(
            R"({"type": "message_part", "chat_id": "c1",
                "message_part": {"part": {"type": "tool-call", "tool_call_id": "tc2"}}})"));
        QCOMPARE(s.tail().parts.size(), 2);
    }

    // -----------------------------------------------------------------------
    // Message finalization supersedes the preview
    // -----------------------------------------------------------------------

    void testMessageSupersedesPreview() {
        ChatSession s(QStringLiteral("c1"));
        QSignalSpy upsertSpy(&s, &ChatSession::messageUpserted);

        s.applyEvent(textDelta(QStringLiteral("c1"), QStringLiteral("Hello")));
        QVERIFY(s.hasTail());

        s.applyEvent(messageEvent(QStringLiteral("c1"), 10, QStringLiteral("Hello world")));
        QVERIFY(!s.hasTail());
        QCOMPARE(s.messages().size(), 1);
        QCOMPARE(s.messages().first().id, qint64(10));
        QCOMPARE(s.afterId(), qint64(10));
        QCOMPARE(upsertSpy.count(), 1);
        const QList<QVariant> args = upsertSpy.takeFirst();
        QCOMPARE(args.at(0).toInt(), 0);       // appended at index 0
        QCOMPARE(args.at(1).toBool(), false);  // not existing
        QCOMPARE(args.at(2).toBool(), true);   // tail was flushed
    }

    void testMessageUpsertReplacesById() {
        ChatSession s(QStringLiteral("c1"));
        s.applyEvent(messageEvent(QStringLiteral("c1"), 10, QStringLiteral("v1")));
        s.applyEvent(messageEvent(QStringLiteral("c1"), 10, QStringLiteral("v2")));
        QCOMPARE(s.messages().size(), 1);
        QCOMPARE(s.messages().first().parts.first().text, QStringLiteral("v2"));
    }

    void testAfterIdCursorAdvancesMonotonically() {
        ChatSession s(QStringLiteral("c1"));
        s.applyEvent(messageEvent(QStringLiteral("c1"), 10, QStringLiteral("a")));
        s.applyEvent(messageEvent(QStringLiteral("c1"), 12, QStringLiteral("b")));
        QCOMPARE(s.afterId(), qint64(12));
        // Re-delivery of an older durable message does not move the cursor back.
        s.applyEvent(messageEvent(QStringLiteral("c1"), 10, QStringLiteral("a2")));
        QCOMPARE(s.afterId(), qint64(12));
    }

    // -----------------------------------------------------------------------
    // preview_reset
    // -----------------------------------------------------------------------

    void testPreviewResetClearsTail() {
        ChatSession s(QStringLiteral("c1"));
        QSignalSpy tailSpy(&s, &ChatSession::tailChanged);
        s.applyEvent(textDelta(QStringLiteral("c1"), QStringLiteral("junk")));
        s.applyEvent(eventFromJson(R"({"type": "preview_reset", "chat_id": "c1"})"));
        QVERIFY(!s.hasTail());
        QCOMPARE(tailSpy.count(), 2);
        // A reset with no tail is a no-op.
        s.applyEvent(eventFromJson(R"({"type": "preview_reset", "chat_id": "c1"})"));
        QCOMPARE(tailSpy.count(), 2);
    }

    // -----------------------------------------------------------------------
    // history_reset atomic replacement
    // -----------------------------------------------------------------------

    void testHistoryResetAtomicReplace() {
        ChatSession s(QStringLiteral("c1"));
        s.applyEvent(messageEvent(QStringLiteral("c1"), 1, QStringLiteral("old1")));
        s.applyEvent(messageEvent(QStringLiteral("c1"), 2, QStringLiteral("old2")));
        s.applyEvent(textDelta(QStringLiteral("c1"), QStringLiteral("streaming")));

        QSignalSpy replaceSpy(&s, &ChatSession::historyReplaced);
        s.applyEvent(eventFromJson(R"({"type": "history_reset", "chat_id": "c1"})"));
        // Replacement messages buffer without touching the visible history.
        s.applyEvent(messageEvent(QStringLiteral("c1"), 5, QStringLiteral("new1")));
        s.applyEvent(messageEvent(QStringLiteral("c1"), 6, QStringLiteral("new2")));
        QCOMPARE(replaceSpy.count(), 0);
        QCOMPARE(s.messages().size(), 2);
        QCOMPARE(s.messages().first().parts.first().text, QStringLiteral("old1"));

        // The next non-message event commits the replacement atomically.
        s.applyEvent(statusEvent(QStringLiteral("c1"), QStringLiteral("running")));
        QCOMPARE(replaceSpy.count(), 1);
        QCOMPARE(s.messages().size(), 2);
        QCOMPARE(s.messages()[0].id, qint64(5));
        QCOMPARE(s.messages()[1].id, qint64(6));
        QVERIFY(!s.hasTail());
        QCOMPARE(s.afterId(), qint64(6));
        // The terminating event itself is processed normally afterwards.
        QCOMPARE(s.status(), ChatStatus::Running);
    }

    void testHistoryResetTerminatedByMessagePart() {
        ChatSession s(QStringLiteral("c1"));
        s.applyEvent(messageEvent(QStringLiteral("c1"), 1, QStringLiteral("old")));
        s.applyEvent(eventFromJson(R"({"type": "history_reset", "chat_id": "c1"})"));
        s.applyEvent(messageEvent(QStringLiteral("c1"), 3, QStringLiteral("new")));
        // A message_part event is a non-message event and terminates the batch.
        s.applyEvent(textDelta(QStringLiteral("c1"), QStringLiteral("tail")));
        QCOMPARE(s.messages().size(), 1);
        QCOMPARE(s.messages().first().id, qint64(3));
        QVERIFY(s.hasTail());
        QCOMPARE(s.tail().parts.first().text, QStringLiteral("tail"));
    }

    void testHistoryResetToEmpty() {
        ChatSession s(QStringLiteral("c1"));
        s.applyEvent(messageEvent(QStringLiteral("c1"), 1, QStringLiteral("old")));
        s.applyEvent(eventFromJson(R"({"type": "history_reset", "chat_id": "c1"})"));
        s.applyEvent(statusEvent(QStringLiteral("c1"), QStringLiteral("waiting")));
        QVERIFY(s.messages().isEmpty());
        QCOMPARE(s.afterId(), qint64(0));
    }

    // -----------------------------------------------------------------------
    // Queue snapshot
    // -----------------------------------------------------------------------

    void testQueueUpdateIsAuthoritative() {
        ChatSession s(QStringLiteral("c1"));
        QSignalSpy queueSpy(&s, &ChatSession::queueChanged);
        s.applyEvent(eventFromJson(
            R"({"type": "queue_update", "chat_id": "c1",
                "queued_messages": [{"id": 5, "chat_id": "c1", "content": []},
                                    {"id": 6, "chat_id": "c1", "content": []}]})"));
        QCOMPARE(s.queuedMessages().size(), 2);
        s.applyEvent(eventFromJson(R"({"type": "queue_update", "chat_id": "c1",
                              "queued_messages": []})"));
        QVERIFY(s.queuedMessages().isEmpty());
        QCOMPARE(queueSpy.count(), 2);
    }

    // -----------------------------------------------------------------------
    // Cross-chat drop
    // -----------------------------------------------------------------------

    void testEventsForOtherChatsAreDropped() {
        ChatSession s(QStringLiteral("c1"));
        QSignalSpy tailSpy(&s, &ChatSession::tailChanged);
        s.applyEvent(textDelta(QStringLiteral("c2"), QStringLiteral("wrong chat")));
        s.applyEvent(messageEvent(QStringLiteral("c2"), 99, QStringLiteral("wrong")));
        s.applyEvent(statusEvent(QStringLiteral("c2"), QStringLiteral("running")));
        QVERIFY(!s.hasTail());
        QVERIFY(s.messages().isEmpty());
        QCOMPARE(s.status(), ChatStatus::Unknown);
        QCOMPARE(tailSpy.count(), 0);
    }

    void testEventWithoutChatIdIsAccepted() {
        // codersdk fills in the chat id when the server omits it; an empty
        // chat_id on the wire is treated as belonging to this stream.
        ChatSession s(QStringLiteral("c1"));
        s.applyEvent(textDelta(QString(), QStringLiteral("hi")));
        QVERIFY(s.hasTail());
    }

    // -----------------------------------------------------------------------
    // Status / error / retry / action_required transitions
    // -----------------------------------------------------------------------

    void testStatusTransitions() {
        ChatSession s(QStringLiteral("c1"));
        QSignalSpy statusSpy(&s, &ChatSession::statusChanged);
        s.applyEvent(statusEvent(QStringLiteral("c1"), QStringLiteral("running")));
        s.applyEvent(statusEvent(QStringLiteral("c1"), QStringLiteral("running")));
        s.applyEvent(statusEvent(QStringLiteral("c1"), QStringLiteral("completed")));
        QCOMPARE(s.status(), ChatStatus::Completed);
        // Repeated identical statuses do not re-signal.
        QCOMPARE(statusSpy.count(), 2);
        QCOMPARE(statusSpy.at(1).at(0).value<ChatStatus>(), ChatStatus::Running);
        QCOMPARE(statusSpy.at(1).at(1).value<ChatStatus>(), ChatStatus::Completed);
    }

    void testErrorRetryAndActionRequiredState() {
        ChatSession s(QStringLiteral("c1"));
        QSignalSpy errorSpy(&s, &ChatSession::errorChanged);
        QSignalSpy retrySpy(&s, &ChatSession::retryChanged);
        QSignalSpy actionSpy(&s, &ChatSession::actionRequiredChanged);

        s.applyEvent(eventFromJson(
            R"({"type": "error", "chat_id": "c1",
                "error": {"message": "boom", "kind": "generic", "retryable": false}})"));
        QVERIFY(s.hasError());
        QCOMPARE(s.lastError().message, QStringLiteral("boom"));
        QCOMPARE(errorSpy.count(), 1);

        s.applyEvent(eventFromJson(
            R"({"type": "retry", "chat_id": "c1",
                "retry": {"attempt": 1, "delay_ms": 1000, "error": "overload"}})"));
        QVERIFY(s.hasRetry());
        QCOMPARE(s.retry().attempt, 1);
        QCOMPARE(retrySpy.count(), 1);

        s.applyEvent(eventFromJson(
            R"({"type": "action_required", "chat_id": "c1",
                "action_required": {"tool_calls": [
                    {"tool_call_id": "tc1", "tool_name": "confirm", "args": "{}"}]}})"));
        QVERIFY(s.hasActionRequired());
        QCOMPARE(s.actionRequired().toolCalls.size(), 1);
        QCOMPARE(actionSpy.count(), 1);
    }

    // -----------------------------------------------------------------------
    // Initial load and pagination plumbing
    // -----------------------------------------------------------------------

    void testInitialMessagesSortedAndCursor() {
        ChatSession s(QStringLiteral("c1"));
        const QList<ChatMessage> page = {
            ChatMessage::fromJson(
                QJsonDocument::fromJson(R"({"id": 7, "chat_id": "c1", "role": "user"})").object()),
            ChatMessage::fromJson(
                QJsonDocument::fromJson(R"({"id": 3, "chat_id": "c1", "role": "user"})").object()),
        };
        s.setInitialMessages(page);
        QCOMPARE(s.messages().size(), 2);
        QCOMPARE(s.messages()[0].id, qint64(3));
        QCOMPARE(s.messages()[1].id, qint64(7));
        QCOMPARE(s.afterId(), qint64(7));
    }

    void testInitialMessagesMergePreservesStreamApplied() {
        // The stream (after_id=0) opens concurrently with the initial REST
        // page; durable messages already applied from stream events must
        // survive the page landing when their ids are not covered by it.
        ChatSession s(QStringLiteral("c1"));
        s.applyEvent(messageEvent(QStringLiteral("c1"), 8, QStringLiteral("streamed")));
        s.applyEvent(messageEvent(QStringLiteral("c1"), 5, QStringLiteral("stream v1")));

        const QList<ChatMessage> page = {
            ChatMessage::fromJson(QJsonDocument::fromJson(
                                      R"({"id": 5, "chat_id": "c1", "role": "assistant",
                    "content": [{"type": "text", "text": "page v2"}]})")
                                      .object()),
            ChatMessage::fromJson(
                QJsonDocument::fromJson(R"({"id": 3, "chat_id": "c1", "role": "user"})").object()),
        };
        s.setInitialMessages(page);

        QCOMPARE(s.messages().size(), 3);
        QCOMPARE(s.messages()[0].id, qint64(3));
        QCOMPARE(s.messages()[1].id, qint64(5));
        // The page wins for ids it covers.
        QCOMPARE(s.messages()[1].parts.first().text, QStringLiteral("page v2"));
        // The stream-applied message not covered by the page is preserved.
        QCOMPARE(s.messages()[2].id, qint64(8));
        QCOMPARE(s.afterId(), qint64(8));
    }

    void testPrependOlderDropsDuplicates() {
        ChatSession s(QStringLiteral("c1"));
        s.applyEvent(messageEvent(QStringLiteral("c1"), 10, QStringLiteral("newest")));
        QSignalSpy olderSpy(&s, &ChatSession::olderMessagesPrepended);

        const QList<ChatMessage> older = {
            ChatMessage::fromJson(
                QJsonDocument::fromJson(R"({"id": 10, "chat_id": "c1", "role": "user"})").object()),
            ChatMessage::fromJson(
                QJsonDocument::fromJson(R"({"id": 4, "chat_id": "c1", "role": "user"})").object()),
            ChatMessage::fromJson(
                QJsonDocument::fromJson(R"({"id": 8, "chat_id": "c1", "role": "user"})").object()),
        };
        s.prependOlderMessages(older);
        QCOMPARE(s.messages().size(), 3);
        QCOMPARE(s.messages()[0].id, qint64(4));
        QCOMPARE(s.messages()[1].id, qint64(8));
        QCOMPARE(s.messages()[2].id, qint64(10));
        QCOMPARE(olderSpy.count(), 1);
        QCOMPARE(olderSpy.first().first().toInt(), 2);
    }

    // -----------------------------------------------------------------------
    // Stream replay ordering (regression)
    // -----------------------------------------------------------------------

    // Regression: a stream socket that connected with after_id=0 replays the
    // full history as message events. When the REST page (the NEWEST ids,
    // e.g. 5158447..5164185 on dev.coder.com chat 0cd036c5) lands mid-replay,
    // later-replayed OLDER messages must insert in id order instead of being
    // appended past the newest ids.
    void testStreamReplayOlderMessageInsertsInIdOrder() {
        ChatSession s(QStringLiteral("0cd036c5-0961-40fe-93ab-1eef442c875e"));
        QSignalSpy upsertSpy(&s, &ChatSession::messageUpserted);

        // Initial REST page: the two newest durable messages (real ids and
        // timestamp format from the live payload).
        const QList<ChatMessage> page = {
            ChatMessage::fromJson(QJsonDocument::fromJson(R"({
                "id": 5164185,
                "chat_id": "0cd036c5-0961-40fe-93ab-1eef442c875e",
                "created_at": "2026-07-13T14:52:15.612947Z",
                "role": "assistant",
                "content": [{"type": "text", "text": "newest"}]})")
                                      .object()),
            ChatMessage::fromJson(QJsonDocument::fromJson(R"({
                "id": 5158447,
                "chat_id": "0cd036c5-0961-40fe-93ab-1eef442c875e",
                "created_at": "2026-07-13T12:55:01.960083Z",
                "role": "assistant",
                "content": [{"type": "text", "text": "older page entry"}]})")
                                      .object()),
        };
        s.setInitialMessages(page);
        QCOMPARE(s.afterId(), qint64(5164185));

        // Stream replay continues with a message OLDER than the whole page.
        s.applyEvent(messageEvent(QStringLiteral("0cd036c5-0961-40fe-93ab-1eef442c875e"), 5150000,
                                  QStringLiteral("replayed old")));

        QCOMPARE(s.messages().size(), 3);
        QCOMPARE(s.messages()[0].id, qint64(5150000));
        QCOMPARE(s.messages()[1].id, qint64(5158447));
        QCOMPARE(s.messages()[2].id, qint64(5164185));
        // The cursor stays at the newest durable id.
        QCOMPARE(s.afterId(), qint64(5164185));
        // The upsert reported the actual insertion index (0, oldest end).
        QCOMPARE(upsertSpy.count(), 1);
        QCOMPARE(upsertSpy.first().at(0).toInt(), 0);
        QCOMPARE(upsertSpy.first().at(1).toBool(), false);
    }
};

QTEST_MAIN(TestChatSession)
#include "tst_chatsession.moc"
