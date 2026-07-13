#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTest>

#include "api/dto/Chat.h"
#include "api/dto/ChatMessage.h"
#include "api/dto/ChatModels.h"
#include "api/dto/ChatStreamEvent.h"

/// Unit tests for the Coder Agents DTO parsing: every message part type,
/// unknown-part retention and round-trip, stream frame parsing, and watch
/// event parsing.
class TestAgentDtos : public QObject {
    Q_OBJECT

    static QJsonObject parseObj(const char* json) { return QJsonDocument::fromJson(json).object(); }

private slots:
    // -----------------------------------------------------------------------
    // Chat
    // -----------------------------------------------------------------------

    void testChatFromJson() {
        const QJsonObject obj = parseObj(R"({
            "id": "c1",
            "organization_id": "org1",
            "owner_id": "u1",
            "workspace_id": "ws1",
            "last_model_config_id": "mc1",
            "title": "Fix the bug",
            "status": "running",
            "plan_mode": "plan",
            "last_error": {"message": "boom", "kind": "rate_limit", "retryable": true,
                           "status_code": 429},
            "diff_status": {"additions": 12, "deletions": 3, "changed_files": 2,
                            "pr_number": 42, "url": "x.test/pr/42"},
            "created_at": "2026-07-01T10:00:00Z",
            "updated_at": "2026-07-02T11:30:00Z",
            "archived": false,
            "pin_order": 2,
            "labels": {"team": "infra"},
            "has_unread": true,
            "children": [
                {"id": "c2", "parent_chat_id": "c1", "title": "sub", "status": "completed",
                 "children": []}
            ]
        })");

        const Chat c = Chat::fromJson(obj);
        QCOMPARE(c.id, QStringLiteral("c1"));
        QCOMPARE(c.organizationId, QStringLiteral("org1"));
        QCOMPARE(c.workspaceId, QStringLiteral("ws1"));
        QCOMPARE(c.lastModelConfigId, QStringLiteral("mc1"));
        QCOMPARE(c.title, QStringLiteral("Fix the bug"));
        QCOMPARE(c.status, ChatStatus::Running);
        QCOMPARE(c.statusString, QStringLiteral("running"));
        QCOMPARE(c.planMode, QStringLiteral("plan"));
        QVERIFY(c.hasLastError);
        QCOMPARE(c.lastError.message, QStringLiteral("boom"));
        QCOMPARE(c.lastError.kind, QStringLiteral("rate_limit"));
        QVERIFY(c.lastError.retryable);
        QCOMPARE(c.lastError.statusCode, 429);
        QVERIFY(c.diffStatus.hasValue);
        QCOMPARE(c.diffStatus.additions, 12);
        QCOMPARE(c.diffStatus.deletions, 3);
        QCOMPARE(c.diffStatus.prNumber, 42);
        QVERIFY(c.createdAt.isValid());
        QVERIFY(c.updatedAt.isValid());
        QVERIFY(!c.archived);
        QCOMPARE(c.pinOrder, 2);
        QVERIFY(c.isPinned());
        QCOMPARE(c.labels.value(QStringLiteral("team")).toString(), QStringLiteral("infra"));
        QVERIFY(c.hasUnread);
        QCOMPARE(c.children.size(), 1);
        QCOMPARE(c.children.first().id, QStringLiteral("c2"));
        QVERIFY(c.children.first().isSubagent());
        QVERIFY(!c.isSubagent());
    }

    void testChatStatusFallback() {
        QCOMPARE(chatStatusFromString(QStringLiteral("some_future_status")), ChatStatus::Unknown);
        QCOMPARE(chatStatusFromString(QStringLiteral("waiting")), ChatStatus::Waiting);
        QCOMPARE(chatStatusFromString(QStringLiteral("pending")), ChatStatus::Pending);
        QCOMPARE(chatStatusFromString(QStringLiteral("paused")), ChatStatus::Paused);
        QCOMPARE(chatStatusFromString(QStringLiteral("completed")), ChatStatus::Completed);
        QCOMPARE(chatStatusFromString(QStringLiteral("error")), ChatStatus::Error);
        QCOMPARE(chatStatusFromString(QStringLiteral("requires_action")),
                 ChatStatus::RequiresAction);
        QCOMPARE(chatStatusFromString(QStringLiteral("interrupting")), ChatStatus::Interrupting);
    }

    void testChatListFromJson() {
        const QJsonDocument doc = QJsonDocument::fromJson(
            R"([{"id": "a", "status": "waiting", "children": []},
                {"id": "b", "status": "completed", "children": []}])");
        const QList<Chat> chats = Chat::listFromJson(doc.array());
        QCOMPARE(chats.size(), 2);
        QCOMPARE(chats[0].id, QStringLiteral("a"));
        QCOMPARE(chats[1].status, ChatStatus::Completed);
    }

    // -----------------------------------------------------------------------
    // ChatMessagePart: every known type
    // -----------------------------------------------------------------------

    void testTextPart() {
        const ChatMessagePart p =
            ChatMessagePart::fromJson(parseObj(R"({"type": "text", "text": "hello"})"));
        QCOMPARE(p.type, ChatMessagePartType::Text);
        QCOMPARE(p.text, QStringLiteral("hello"));
        const QJsonObject out = p.toJson();
        QCOMPARE(out.value("type").toString(), QStringLiteral("text"));
        QCOMPARE(out.value("text").toString(), QStringLiteral("hello"));
    }

    void testReasoningPart() {
        const ChatMessagePart p = ChatMessagePart::fromJson(parseObj(
            R"({"type": "reasoning", "text": "thinking", "signature": "sig",
                "created_at": "2026-07-01T10:00:00Z",
                "completed_at": "2026-07-01T10:00:05Z"})"));
        QCOMPARE(p.type, ChatMessagePartType::Reasoning);
        QCOMPARE(p.text, QStringLiteral("thinking"));
        QCOMPARE(p.signature, QStringLiteral("sig"));
        QVERIFY(p.createdAt.isValid());
        QVERIFY(p.completedAt.isValid());
        QCOMPARE(p.createdAt.msecsTo(p.completedAt), qint64(5000));
    }

    void testToolCallPart() {
        const ChatMessagePart p = ChatMessagePart::fromJson(parseObj(
            R"({"type": "tool-call", "tool_call_id": "tc1", "tool_name": "execute",
                "args": {"command": "ls", "model_intent": "list files"},
                "args_delta": "partial-args", "provider_executed": true})"));
        QCOMPARE(p.type, ChatMessagePartType::ToolCall);
        QCOMPARE(p.toolCallId, QStringLiteral("tc1"));
        QCOMPARE(p.toolName, QStringLiteral("execute"));
        QCOMPARE(p.args.value("command").toString(), QStringLiteral("ls"));
        QCOMPARE(p.argsDelta, QStringLiteral("partial-args"));
        QVERIFY(p.providerExecuted);
        const QJsonObject out = p.toJson();
        QCOMPARE(out.value("tool_call_id").toString(), QStringLiteral("tc1"));
        QCOMPARE(out.value("args").toObject().value("command").toString(), QStringLiteral("ls"));
    }

    void testToolResultPart() {
        const ChatMessagePart p = ChatMessagePart::fromJson(parseObj(
            R"({"type": "tool-result", "tool_call_id": "tc1", "tool_name": "execute",
                "result": "file1\nfile2", "result_delta": "fil", "is_error": true})"));
        QCOMPARE(p.type, ChatMessagePartType::ToolResult);
        QCOMPARE(p.toolCallId, QStringLiteral("tc1"));
        QCOMPARE(p.result.toString(), QStringLiteral("file1\nfile2"));
        QCOMPARE(p.resultDelta, QStringLiteral("fil"));
        QVERIFY(p.isError);
        const QJsonObject out = p.toJson();
        QVERIFY(out.value("is_error").toBool());
        QCOMPARE(out.value("result").toString(), QStringLiteral("file1\nfile2"));
    }

    void testSourcePart() {
        const ChatMessagePart p = ChatMessagePart::fromJson(parseObj(
            R"({"type": "source", "source_id": "s1", "url": "example.com/doc",
                "title": "Example"})"));
        QCOMPARE(p.type, ChatMessagePartType::Source);
        QCOMPARE(p.sourceId, QStringLiteral("s1"));
        QCOMPARE(p.url, QStringLiteral("example.com/doc"));
        QCOMPARE(p.title, QStringLiteral("Example"));
    }

    void testFilePart() {
        const ChatMessagePart p = ChatMessagePart::fromJson(parseObj(
            R"({"type": "file", "media_type": "image/png", "name": "shot.png",
                "file_id": "f1"})"));
        QCOMPARE(p.type, ChatMessagePartType::File);
        QCOMPARE(p.mediaType, QStringLiteral("image/png"));
        QCOMPARE(p.name, QStringLiteral("shot.png"));
        QCOMPARE(p.fileId, QStringLiteral("f1"));
    }

    void testFileReferencePart() {
        const ChatMessagePart p = ChatMessagePart::fromJson(parseObj(
            R"({"type": "file-reference", "file_name": "main.go", "start_line": 3,
                "end_line": 9, "content": "func main() {}"})"));
        QCOMPARE(p.type, ChatMessagePartType::FileReference);
        QCOMPARE(p.fileName, QStringLiteral("main.go"));
        QCOMPARE(p.startLine, 3);
        QCOMPARE(p.endLine, 9);
        QCOMPARE(p.content, QStringLiteral("func main() {}"));
    }

    void testContextFilePart() {
        const ChatMessagePart p = ChatMessagePart::fromJson(parseObj(
            R"({"type": "context-file", "context_file_path": "/repo/AGENTS.md",
                "context_file_truncated": true})"));
        QCOMPARE(p.type, ChatMessagePartType::ContextFile);
        QCOMPARE(p.contextFilePath, QStringLiteral("/repo/AGENTS.md"));
        QVERIFY(p.contextFileTruncated);
    }

    void testSkillPart() {
        const ChatMessagePart p = ChatMessagePart::fromJson(parseObj(
            R"({"type": "skill", "skill_name": "write-docs",
                "skill_description": "Docs authoring"})"));
        QCOMPARE(p.type, ChatMessagePartType::Skill);
        QCOMPARE(p.skillName, QStringLiteral("write-docs"));
        QCOMPARE(p.skillDescription, QStringLiteral("Docs authoring"));
    }

    void testImagePart() {
        const ChatMessagePart p = ChatMessagePart::fromJson(
            parseObj(R"({"type": "image", "media_type": "image/jpeg", "file_id": "f2"})"));
        QCOMPARE(p.type, ChatMessagePartType::Image);
        QCOMPARE(p.mediaType, QStringLiteral("image/jpeg"));
    }

    void testUnknownPartRetentionAndRoundTrip() {
        const QJsonObject original =
            parseObj(R"({"type": "hologram", "wavelength_nm": 532, "payload": {"a": [1, 2, 3]}})");
        const ChatMessagePart p = ChatMessagePart::fromJson(original);
        QCOMPARE(p.type, ChatMessagePartType::Unknown);
        QCOMPARE(p.typeString, QStringLiteral("hologram"));
        // The raw wire object is retained and round-trips losslessly.
        QCOMPARE(p.raw, original);
        QCOMPARE(p.toJson(), original);
    }

    // -----------------------------------------------------------------------
    // ChatMessage / usage / queued messages
    // -----------------------------------------------------------------------

    void testChatMessageFromJson() {
        const ChatMessage m = ChatMessage::fromJson(parseObj(
            R"({"id": 9007199254740993, "chat_id": "c1", "role": "assistant",
                "created_at": "2026-07-01T10:00:00Z",
                "content": [{"type": "text", "text": "hi"}],
                "usage": {"total_tokens": 1234, "context_limit": 200000}})"));
        // int64 ids survive above the 32-bit range (values beyond 2^53 lose
        // precision in JSON doubles, but the magnitude is preserved).
        QVERIFY(m.id > qint64(9000000000000000000LL / 1000));
        QCOMPARE(m.chatId, QStringLiteral("c1"));
        QCOMPARE(m.role, QStringLiteral("assistant"));
        QCOMPARE(m.parts.size(), 1);
        QVERIFY(m.usage.hasValue);
        QCOMPARE(m.usage.totalTokens, qint64(1234));
        QCOMPARE(m.usage.contextLimit, qint64(200000));
    }

    void testChatMessageUsageAbsent() {
        const ChatMessage m = ChatMessage::fromJson(
            parseObj(R"({"id": 1, "chat_id": "c1", "role": "user", "content": []})"));
        QVERIFY(!m.usage.hasValue);
        QCOMPARE(m.usage.totalTokens, qint64(-1));
        QCOMPARE(m.usage.contextLimit, qint64(-1));
    }

    void testQueuedMessageFromJson() {
        const ChatQueuedMessage q = ChatQueuedMessage::fromJson(parseObj(
            R"({"id": 7, "chat_id": "c1", "content": [{"type": "text", "text": "later"}],
                "created_at": "2026-07-01T10:00:00Z"})"));
        QCOMPARE(q.id, qint64(7));
        QCOMPARE(q.chatId, QStringLiteral("c1"));
        QCOMPARE(q.content.size(), 1);
    }

    // -----------------------------------------------------------------------
    // Stream frame parsing
    // -----------------------------------------------------------------------

    void testParseArrayFrame() {
        bool ok = false;
        const QList<ChatStreamEvent> events = parseChatStreamFrame(
            QStringLiteral(
                R"([{"type": "status", "chat_id": "c1", "status": {"status": "running"}},
                    {"type": "message_part", "chat_id": "c1",
                     "message_part": {"role": "assistant", "seq": 3,
                                      "part": {"type": "text", "text": "hi"}}}])"),
            &ok);
        QVERIFY(ok);
        QCOMPARE(events.size(), 2);
        QCOMPARE(events[0].type, ChatStreamEventType::Status);
        QVERIFY(events[0].hasStatus);
        QCOMPARE(events[0].status, ChatStatus::Running);
        QCOMPARE(events[1].type, ChatStreamEventType::MessagePart);
        QVERIFY(events[1].hasMessagePart);
        QCOMPARE(events[1].messagePart.role, QStringLiteral("assistant"));
        QCOMPARE(events[1].messagePart.seq, qint64(3));
        QCOMPARE(events[1].messagePart.part.text, QStringLiteral("hi"));
    }

    void testParseSingleObjectFrame() {
        bool ok = false;
        const QList<ChatStreamEvent> events =
            parseChatStreamFrame(QStringLiteral(
                                     R"({"type": "error", "chat_id": "c1",
                    "error": {"message": "overload", "kind": "overloaded",
                              "retryable": true, "status_code": 529}})"),
                                 &ok);
        QVERIFY(ok);
        QCOMPARE(events.size(), 1);
        QCOMPARE(events[0].type, ChatStreamEventType::Error);
        QVERIFY(events[0].hasError);
        QCOMPARE(events[0].error.kind, QStringLiteral("overloaded"));
        QVERIFY(events[0].error.retryable);
        QCOMPARE(events[0].error.statusCode, 529);
    }

    void testParseEmptyHeartbeatFrame() {
        bool ok = false;
        const QList<ChatStreamEvent> events = parseChatStreamFrame(QStringLiteral("[]"), &ok);
        QVERIFY(ok);
        QVERIFY(events.isEmpty());
    }

    void testParseInvalidFrame() {
        bool ok = true;
        const QList<ChatStreamEvent> events = parseChatStreamFrame(QStringLiteral("not json"), &ok);
        QVERIFY(!ok);
        QVERIFY(events.isEmpty());
    }

    void testParseRetryEvent() {
        bool ok = false;
        const QList<ChatStreamEvent> events =
            parseChatStreamFrame(QStringLiteral(
                                     R"([{"type": "retry", "chat_id": "c1",
                     "retry": {"attempt": 2, "delay_ms": 4000, "error": "timeout",
                               "kind": "timeout", "retrying_at": "2026-07-01T10:00:04Z"}}])"),
                                 &ok);
        QVERIFY(ok);
        QCOMPARE(events.size(), 1);
        QVERIFY(events[0].hasRetry);
        QCOMPARE(events[0].retry.attempt, 2);
        QCOMPARE(events[0].retry.delayMs, qint64(4000));
        QCOMPARE(events[0].retry.kind, QStringLiteral("timeout"));
        QVERIFY(events[0].retry.retryingAt.isValid());
    }

    void testParseActionRequiredEvent() {
        bool ok = false;
        const QList<ChatStreamEvent> events =
            parseChatStreamFrame(QStringLiteral(
                                     R"([{"type": "action_required", "chat_id": "c1",
                     "action_required": {"tool_calls": [
                        {"tool_call_id": "tc1", "tool_name": "ask_user",
                         "args": "{'question': 'yes?'}"}]}}])"),
                                 &ok);
        QVERIFY(ok);
        QVERIFY(events[0].hasActionRequired);
        QCOMPARE(events[0].actionRequired.toolCalls.size(), 1);
        QCOMPARE(events[0].actionRequired.toolCalls[0].toolName, QStringLiteral("ask_user"));
        QCOMPARE(events[0].actionRequired.toolCalls[0].args,
                 QStringLiteral("{'question': 'yes?'}"));
    }

    void testParseQueueUpdateEvent() {
        bool ok = false;
        const QList<ChatStreamEvent> events =
            parseChatStreamFrame(QStringLiteral(
                                     R"([{"type": "queue_update", "chat_id": "c1",
                     "queued_messages": [{"id": 5, "chat_id": "c1", "content": []}]}])"),
                                 &ok);
        QVERIFY(ok);
        QCOMPARE(events[0].type, ChatStreamEventType::QueueUpdate);
        QCOMPARE(events[0].queuedMessages.size(), 1);
        QCOMPARE(events[0].queuedMessages[0].id, qint64(5));
    }

    // -----------------------------------------------------------------------
    // Watch event
    // -----------------------------------------------------------------------

    void testWatchEventFromJson() {
        const ChatWatchEvent e = ChatWatchEvent::fromJson(parseObj(
            R"({"kind": "status_change",
                "chat": {"id": "c1", "title": "t", "status": "completed", "children": []}})"));
        QCOMPARE(e.kind, QStringLiteral("status_change"));
        QCOMPARE(e.chat.id, QStringLiteral("c1"));
        QCOMPARE(e.chat.status, ChatStatus::Completed);
    }

    // -----------------------------------------------------------------------
    // Models / configs / MCP / usage limits
    // -----------------------------------------------------------------------

    void testModelsResponse() {
        const ChatModelsResponse r = ChatModelsResponse::fromJson(parseObj(
            R"({"providers": [
                 {"provider": "anthropic", "available": true,
                  "models": [{"id": "m1", "provider": "anthropic",
                              "model": "claude", "display_name": "Claude"}]},
                 {"provider": "openai", "available": false,
                  "unavailable_reason": "missing_api_key", "models": []}]})"));
        QCOMPARE(r.providers.size(), 2);
        QVERIFY(r.anyAvailable());
        QCOMPARE(r.providers[0].models[0].displayName, QStringLiteral("Claude"));
        QCOMPARE(r.providers[1].unavailableReason, QStringLiteral("missing_api_key"));
    }

    void testModelConfig() {
        const ChatModelConfig c = ChatModelConfig::fromJson(parseObj(
            R"({"id": "mc1", "ai_provider_id": "p1", "model": "claude",
                "display_name": "Claude", "enabled": true, "is_default": true,
                "context_limit": 200000})"));
        QCOMPARE(c.id, QStringLiteral("mc1"));
        QVERIFY(c.enabled);
        QVERIFY(c.isDefault);
        QCOMPARE(c.contextLimit, qint64(200000));
    }

    void testMcpServerConfig() {
        const McpServerConfig s = McpServerConfig::fromJson(parseObj(
            R"({"id": "s1", "display_name": "GitHub", "slug": "github",
                "transport": "streamable_http", "url": "mcp.example.test",
                "auth_type": "oauth2", "auth_connected": true,
                "availability": "force_on", "enabled": true,
                "allow_in_plan_mode": false})"));
        QCOMPARE(s.slug, QStringLiteral("github"));
        QCOMPARE(s.authType, QStringLiteral("oauth2"));
        QVERIFY(s.authConnected);
        QCOMPARE(s.availability, QStringLiteral("force_on"));
        QVERIFY(!s.allowInPlanMode);
    }

    void testUsageLimitStatus() {
        const ChatUsageLimitStatus s = ChatUsageLimitStatus::fromJson(parseObj(
            R"({"is_limited": true, "period": "monthly", "spend_limit_micros": 50000000,
                "current_spend": 12000000, "period_start": "2026-07-01T00:00:00Z",
                "period_end": "2026-08-01T00:00:00Z"})"));
        QVERIFY(s.isLimited);
        QCOMPARE(s.spendLimitMicros, qint64(50000000));
        QCOMPARE(s.currentSpend, qint64(12000000));
        QVERIFY(s.periodStart.isValid());
    }

    void testUsageLimitExceeded409Payload() {
        const ChatUsageLimitExceeded u = ChatUsageLimitExceeded::fromJson(parseObj(
            R"({"message": "Usage limit exceeded.",
                "spent_micros": 51000000, "limit_micros": 50000000,
                "resets_at": "2026-08-01T00:00:00Z"})"));
        QVERIFY(u.isValid());
        QCOMPARE(u.message, QStringLiteral("Usage limit exceeded."));
        QCOMPARE(u.spentMicros, qint64(51000000));
        QCOMPARE(u.limitMicros, qint64(50000000));
        QVERIFY(u.resetsAt.isValid());
    }

    void testUsageLimit409Discrimination() {
        // Mirrors codersdk isChatUsageLimitExceededResponse: only a payload
        // with both a message and a parseable resets_at is a usage-limit
        // response; anything else must fall through to requestFailed.
        QVERIFY(ChatUsageLimitExceeded::fromJson(parseObj(
                                                     R"({"message": "Usage limit exceeded.",
                        "limit_micros": 50000000,
                        "resets_at": "2026-08-01T00:00:00Z"})"))
                    .isUsageLimit());
        // Plain 409 conflict bodies from coderd/exp_chats.go.
        QVERIFY(!ChatUsageLimitExceeded::fromJson(
                     parseObj(R"({"message": "Chat is in an invalid state."})"))
                     .isUsageLimit());
        QVERIFY(
            !ChatUsageLimitExceeded::fromJson(
                 parseObj(R"({"message": "Chat is not in a state that accepts new messages."})"))
                 .isUsageLimit());
        // Missing message, invalid resets_at, and empty/non-JSON bodies.
        QVERIFY(
            !ChatUsageLimitExceeded::fromJson(parseObj(R"({"resets_at": "2026-08-01T00:00:00Z"})"))
                 .isUsageLimit());
        QVERIFY(!ChatUsageLimitExceeded::fromJson(
                     parseObj(R"({"message": "x", "resets_at": "not-a-date"})"))
                     .isUsageLimit());
        QVERIFY(!ChatUsageLimitExceeded::fromJson(QJsonObject{}).isUsageLimit());
    }

    // -----------------------------------------------------------------------
    // Chats-by-workspace map
    // -----------------------------------------------------------------------

    void testParseChatsByWorkspaceMap() {
        const QVariantMap map = parseChatsByWorkspaceMap(parseObj(R"({
            "ws-1": "chat-1",
            "ws-2": "chat-2",
            "ws-bad": 42
        })"));
        QCOMPARE(map.size(), 2);
        QCOMPARE(map.value(QStringLiteral("ws-1")).toString(), QStringLiteral("chat-1"));
        QCOMPARE(map.value(QStringLiteral("ws-2")).toString(), QStringLiteral("chat-2"));
        QVERIFY(!map.contains(QStringLiteral("ws-bad")));
        QVERIFY(parseChatsByWorkspaceMap(QJsonObject{}).isEmpty());
    }
};

QTEST_MAIN(TestAgentDtos)
#include "tst_agentdtos.moc"
