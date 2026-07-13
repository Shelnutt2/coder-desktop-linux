#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTest>

#include "agents/ChatSession.h"
#include "models/ChatListModel.h"
#include "models/ChatMessagesModel.h"

/// Unit tests for ChatListModel (sorting, filtering, time groups, granular
/// updates) and ChatMessagesModel (newest-first ordering, streaming tail
/// lifecycle, delta coalescing, older-page insertion stability).
class TestChatModels : public QObject {
    Q_OBJECT

    static Chat makeChat(const QString& id, const QString& title, const QDateTime& updatedAt,
                         int pinOrder = 0, bool archived = false, bool hasUnread = false,
                         const QString& status = QStringLiteral("waiting")) {
        Chat c;
        c.id = id;
        c.title = title;
        c.updatedAt = updatedAt;
        c.pinOrder = pinOrder;
        c.archived = archived;
        c.hasUnread = hasUnread;
        c.statusString = status;
        c.status = chatStatusFromString(status);
        return c;
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

    static ChatMessage makeMessage(qint64 id, const QString& text) {
        QJsonObject part{{"type", "text"}, {"text", text}};
        return ChatMessage::fromJson(QJsonObject{{"id", double(id)},
                                                 {"chat_id", "c1"},
                                                 {"role", "user"},
                                                 {"content", QJsonArray{part}}});
    }

    static QString idAt(const ChatListModel& m, int row) {
        return m.data(m.index(row), ChatListModel::IdRole).toString();
    }

private slots:
    // =======================================================================
    // ChatListModel
    // =======================================================================

    void testSortPinnedFirstThenUpdatedAt() {
        ChatListModel model;
        const QDateTime now = QDateTime::currentDateTimeUtc();
        model.setChats({
            makeChat("recent", "Recent", now.addSecs(-60)),
            makeChat("pin2", "Pin two", now.addDays(-3), 2),
            makeChat("old", "Old", now.addDays(-10)),
            makeChat("pin1", "Pin one", now.addDays(-5), 1),
        });
        QCOMPARE(model.rowCount(), 4);
        QCOMPARE(idAt(model, 0), QStringLiteral("pin1"));
        QCOMPARE(idAt(model, 1), QStringLiteral("pin2"));
        QCOMPARE(idAt(model, 2), QStringLiteral("recent"));
        QCOMPARE(idAt(model, 3), QStringLiteral("old"));
    }

    void testSubagentsFollowParent() {
        ChatListModel model;
        const QDateTime now = QDateTime::currentDateTimeUtc();
        Chat parent = makeChat("p", "Parent", now);
        Chat child = makeChat("child", "Child", now.addSecs(-10));
        child.parentChatId = QStringLiteral("p");
        parent.children = {child};
        model.setChats({makeChat("other", "Other", now.addSecs(-5)), parent});

        QCOMPARE(model.rowCount(), 3);
        QCOMPARE(idAt(model, 0), QStringLiteral("p"));
        QCOMPARE(idAt(model, 1), QStringLiteral("child"));
        QVERIFY(model.data(model.index(1), ChatListModel::IsSubagentRole).toBool());
        QCOMPARE(model.data(model.index(1), ChatListModel::ParentIdRole).toString(),
                 QStringLiteral("p"));
        QCOMPARE(model.data(model.index(0), ChatListModel::ChildCountRole).toInt(), 1);
        QCOMPARE(idAt(model, 2), QStringLiteral("other"));
    }

    void testFilters() {
        ChatListModel model;
        const QDateTime now = QDateTime::currentDateTimeUtc();
        model.setChats({
            makeChat("a", "A", now, 0, false, true),
            makeChat("b", "B", now.addSecs(-1), 0, true),
            makeChat("c", "C", now.addSecs(-2), 0, false, false, QStringLiteral("requires_action")),
        });
        // All excludes archived chats.
        QCOMPARE(model.rowCount(), 2);

        model.setFilter(ChatListModel::Filter::Unread);
        QCOMPARE(model.rowCount(), 1);
        QCOMPARE(idAt(model, 0), QStringLiteral("a"));

        model.setFilter(ChatListModel::Filter::RequiresAction);
        QCOMPARE(model.rowCount(), 1);
        QCOMPARE(idAt(model, 0), QStringLiteral("c"));
        QVERIFY(model.data(model.index(0), ChatListModel::RequiresActionRole).toBool());

        model.setFilter(ChatListModel::Filter::Archived);
        QCOMPARE(model.rowCount(), 1);
        QCOMPARE(idAt(model, 0), QStringLiteral("b"));
    }

    void testSearchText() {
        ChatListModel model;
        const QDateTime now = QDateTime::currentDateTimeUtc();
        model.setChats({
            makeChat("a", "Fix login bug", now),
            makeChat("b", "Write docs", now.addSecs(-1)),
        });
        model.setSearchText(QStringLiteral("LOGIN"));
        QCOMPARE(model.rowCount(), 1);
        QCOMPARE(idAt(model, 0), QStringLiteral("a"));
        model.setSearchText(QString());
        QCOMPARE(model.rowCount(), 2);
    }

    void testTimeGroups() {
        ChatListModel model;
        const QDateTime now = QDateTime::currentDateTime();
        model.setChats({
            makeChat("pinned", "P", now.addDays(-30), 1),
            makeChat("today", "T", now),
            makeChat("yesterday", "Y", now.addDays(-1)),
            makeChat("week", "W", now.addDays(-5)),
            makeChat("older", "O", now.addDays(-400)),
        });
        const auto groupOf = [&](int row) {
            return model.data(model.index(row), ChatListModel::TimeGroupRole).toString();
        };
        QCOMPARE(groupOf(0), QStringLiteral("Pinned"));
        QCOMPARE(groupOf(1), QStringLiteral("Today"));
        QCOMPARE(groupOf(2), QStringLiteral("Yesterday"));
        QCOMPARE(groupOf(3), QStringLiteral("This Week"));
        QCOMPARE(groupOf(4), QStringLiteral("Older"));
    }

    void testUpsertIsGranularNoReset() {
        ChatListModel model;
        const QDateTime now = QDateTime::currentDateTimeUtc();
        model.setChats({
            makeChat("a", "A", now.addSecs(-10)),
            makeChat("b", "B", now.addSecs(-20)),
        });

        QSignalSpy resetSpy(&model, &QAbstractItemModel::modelReset);
        QSignalSpy insertSpy(&model, &QAbstractItemModel::rowsInserted);
        QSignalSpy removeSpy(&model, &QAbstractItemModel::rowsRemoved);
        QSignalSpy dataSpy(&model, &QAbstractItemModel::dataChanged);

        // In-place update: same position, changed title.
        Chat a2 = makeChat("a", "A renamed", now.addSecs(-10));
        model.upsertChat(a2);
        QCOMPARE(resetSpy.count(), 0);
        QCOMPARE(insertSpy.count(), 0);
        QCOMPARE(removeSpy.count(), 0);
        QVERIFY(dataSpy.count() >= 1);
        QCOMPARE(model.data(model.index(0), ChatListModel::TitleRole).toString(),
                 QStringLiteral("A renamed"));

        // New chat with the newest updatedAt lands at row 0 via insert.
        model.upsertChat(makeChat("c", "C", now));
        QCOMPARE(resetSpy.count(), 0);
        QCOMPARE(insertSpy.count(), 1);
        QCOMPARE(idAt(model, 0), QStringLiteral("c"));
        QCOMPARE(model.rowCount(), 3);

        // Bumping b's updatedAt reorders it to the top without a reset.
        model.upsertChat(makeChat("b", "B", now.addSecs(5)));
        QCOMPARE(resetSpy.count(), 0);
        QCOMPARE(idAt(model, 0), QStringLiteral("b"));
        QCOMPARE(model.rowCount(), 3);

        // Removing a chat is granular too.
        model.removeChat(QStringLiteral("a"));
        QCOMPARE(resetSpy.count(), 0);
        QCOMPARE(model.rowCount(), 2);
    }

    void testSetChatsAppliesGranularlyNoReset() {
        // The 15s polling fallback replaces the whole list through
        // setChats(); it must sync granularly so the view keeps its scroll
        // position and selection.
        ChatListModel model;
        const QDateTime now = QDateTime::currentDateTimeUtc();
        model.setChats({
            makeChat("a", "A", now.addSecs(-10)),
            makeChat("b", "B", now.addSecs(-20)),
        });
        QCOMPARE(model.rowCount(), 2);

        QSignalSpy resetSpy(&model, &QAbstractItemModel::modelReset);
        QSignalSpy insertSpy(&model, &QAbstractItemModel::rowsInserted);
        QSignalSpy dataSpy(&model, &QAbstractItemModel::dataChanged);

        // Changed content: a retitled, b unchanged, c added at the top.
        model.setChats({
            makeChat("a", "A renamed", now.addSecs(-10)),
            makeChat("b", "B", now.addSecs(-20)),
            makeChat("c", "C", now),
        });

        QCOMPARE(resetSpy.count(), 0);
        QCOMPARE(insertSpy.count(), 1);
        QVERIFY(dataSpy.count() >= 1);
        QCOMPARE(model.rowCount(), 3);
        QCOMPARE(idAt(model, 0), QStringLiteral("c"));
        QCOMPARE(model.data(model.index(1), ChatListModel::TitleRole).toString(),
                 QStringLiteral("A renamed"));

        // Shrinking the list is granular too.
        QSignalSpy removeSpy(&model, &QAbstractItemModel::rowsRemoved);
        model.setChats({makeChat("c", "C", now)});
        QCOMPARE(resetSpy.count(), 0);
        QCOMPARE(removeSpy.count(), 2);
        QCOMPARE(model.rowCount(), 1);
        QCOMPARE(idAt(model, 0), QStringLiteral("c"));
    }

    void testUpsertArchivedLeavesDefaultFilter() {
        ChatListModel model;
        const QDateTime now = QDateTime::currentDateTimeUtc();
        model.setChats({makeChat("a", "A", now)});
        QCOMPARE(model.rowCount(), 1);
        QSignalSpy resetSpy(&model, &QAbstractItemModel::modelReset);
        model.upsertChat(makeChat("a", "A", now, 0, true));
        QCOMPARE(model.rowCount(), 0);
        QCOMPARE(resetSpy.count(), 0);
    }

    void testWorkspaceNames() {
        ChatListModel model;
        Chat c = makeChat("a", "A", QDateTime::currentDateTimeUtc());
        c.workspaceId = QStringLiteral("ws1");
        model.setChats({c});
        model.setWorkspaceNames({{QStringLiteral("ws1"), QStringLiteral("dev-box")}});
        QCOMPARE(model.data(model.index(0), ChatListModel::WorkspaceNameRole).toString(),
                 QStringLiteral("dev-box"));
    }

    // =======================================================================
    // ChatMessagesModel
    // =======================================================================

    void testNewestFirstOrdering() {
        ChatSession session(QStringLiteral("c1"));
        ChatMessagesModel model(&session);
        session.setInitialMessages(
            {makeMessage(1, "oldest"), makeMessage(2, "middle"), makeMessage(3, "newest")});
        QCOMPARE(model.rowCount(), 3);
        // Index 0 is the newest message (BottomToTop ListView).
        QCOMPARE(model.data(model.index(0), ChatMessagesModel::MessageIdRole).toLongLong(),
                 qint64(3));
        QCOMPARE(model.data(model.index(2), ChatMessagesModel::MessageIdRole).toLongLong(),
                 qint64(1));
        const QVariantList parts =
            model.data(model.index(0), ChatMessagesModel::PartsRole).toList();
        QCOMPARE(parts.size(), 1);
        QCOMPARE(parts.first().toMap().value(QStringLiteral("text")).toString(),
                 QStringLiteral("newest"));
    }

    void testTailRowLifecycle() {
        ChatSession session(QStringLiteral("c1"));
        ChatMessagesModel model(&session);
        session.setInitialMessages({makeMessage(1, "hello")});

        QSignalSpy insertSpy(&model, &QAbstractItemModel::rowsInserted);
        QSignalSpy removeSpy(&model, &QAbstractItemModel::rowsRemoved);
        QSignalSpy resetSpy(&model, &QAbstractItemModel::modelReset);

        // First delta inserts the tail row at index 0.
        session.applyEvent(textDelta(QStringLiteral("c1"), QStringLiteral("stre")));
        QCOMPARE(insertSpy.count(), 1);
        QCOMPARE(insertSpy.first().at(1).toInt(), 0);
        QCOMPARE(model.rowCount(), 2);
        QVERIFY(model.data(model.index(0), ChatMessagesModel::IsStreamingRole).toBool());
        QCOMPARE(model.data(model.index(0), ChatMessagesModel::MessageIdRole).toLongLong(),
                 qint64(-1));

        // preview_reset removes the tail row.
        session.applyEvent(
            ChatStreamEvent::fromJson(QJsonObject{{"type", "preview_reset"}, {"chat_id", "c1"}}));
        QCOMPARE(removeSpy.count(), 1);
        QCOMPARE(model.rowCount(), 1);
        QCOMPARE(resetSpy.count(), 0);
    }

    void testStreamingDeltaCoalescing() {
        ChatSession session(QStringLiteral("c1"));
        ChatMessagesModel model(&session);
        session.setInitialMessages({});

        session.applyEvent(textDelta(QStringLiteral("c1"), QStringLiteral("a")));
        QCOMPARE(model.rowCount(), 1);

        QSignalSpy dataSpy(&model, &QAbstractItemModel::dataChanged);
        // A burst of deltas produces at most one dataChanged per flush
        // interval (~50ms).
        for (int i = 0; i < 20; ++i)
            session.applyEvent(textDelta(QStringLiteral("c1"), QStringLiteral("x")));
        QCOMPARE(dataSpy.count(), 0);  // nothing flushed synchronously
        QTest::qWait(150);
        QVERIFY(dataSpy.count() >= 1);
        QVERIFY(dataSpy.count() < 20);
        // The flush targets only the tail row, and only the parts role, so
        // bindings on the other roles skip re-evaluation.
        QCOMPARE(dataSpy.first().at(0).toModelIndex().row(), 0);
        QCOMPARE(dataSpy.first().at(1).toModelIndex().row(), 0);
        QCOMPARE(dataSpy.first().at(2).value<QList<int>>(),
                 QList<int>{ChatMessagesModel::PartsRole});

        // The accumulated text is visible after the flush.
        const QVariantList parts =
            model.data(model.index(0), ChatMessagesModel::PartsRole).toList();
        QCOMPARE(parts.first().toMap().value(QStringLiteral("text")).toString(),
                 QStringLiteral("a") + QString(20, QLatin1Char('x')));
    }

    void testFinalizationConvertsTailInPlace() {
        ChatSession session(QStringLiteral("c1"));
        ChatMessagesModel model(&session);
        session.setInitialMessages({makeMessage(1, "first")});
        session.applyEvent(textDelta(QStringLiteral("c1"), QStringLiteral("Hello")));
        QCOMPARE(model.rowCount(), 2);

        QSignalSpy insertSpy(&model, &QAbstractItemModel::rowsInserted);
        QSignalSpy removeSpy(&model, &QAbstractItemModel::rowsRemoved);
        QSignalSpy resetSpy(&model, &QAbstractItemModel::modelReset);
        QSignalSpy dataSpy(&model, &QAbstractItemModel::dataChanged);

        // The finalizing message converts the tail row to durable in place:
        // no insert, no remove, same row count, one dataChanged on row 0.
        session.applyEvent(messageEvent(QStringLiteral("c1"), 2, QStringLiteral("Hello!")));
        QCOMPARE(insertSpy.count(), 0);
        QCOMPARE(removeSpy.count(), 0);
        QCOMPARE(resetSpy.count(), 0);
        QCOMPARE(dataSpy.count(), 1);
        QCOMPARE(dataSpy.first().at(0).toModelIndex().row(), 0);
        QCOMPARE(model.rowCount(), 2);
        QVERIFY(!model.data(model.index(0), ChatMessagesModel::IsStreamingRole).toBool());
        QCOMPARE(model.data(model.index(0), ChatMessagesModel::MessageIdRole).toLongLong(),
                 qint64(2));
    }

    void testDurableAppendWithoutTail() {
        ChatSession session(QStringLiteral("c1"));
        ChatMessagesModel model(&session);
        session.setInitialMessages({makeMessage(1, "first")});

        QSignalSpy insertSpy(&model, &QAbstractItemModel::rowsInserted);
        session.applyEvent(messageEvent(QStringLiteral("c1"), 2, QStringLiteral("user msg")));
        QCOMPARE(insertSpy.count(), 1);
        QCOMPARE(insertSpy.first().at(1).toInt(), 0);  // newest row index
        QCOMPARE(model.rowCount(), 2);
        QCOMPARE(model.data(model.index(0), ChatMessagesModel::MessageIdRole).toLongLong(),
                 qint64(2));
    }

    void testOlderPageInsertionAtEndIndices() {
        ChatSession session(QStringLiteral("c1"));
        ChatMessagesModel model(&session);
        session.setInitialMessages({makeMessage(10, "ten"), makeMessage(11, "eleven")});
        model.setHasMore(true);

        QSignalSpy requestSpy(&model, &ChatMessagesModel::loadOlderRequested);
        model.loadOlder();
        QCOMPARE(requestSpy.count(), 1);
        QCOMPARE(requestSpy.first().first().toLongLong(), qint64(10));
        QVERIFY(model.isLoadingOlder());
        // The in-flight guard blocks re-entry.
        model.loadOlder();
        QCOMPARE(requestSpy.count(), 1);

        QSignalSpy insertSpy(&model, &QAbstractItemModel::rowsInserted);
        QSignalSpy resetSpy(&model, &QAbstractItemModel::modelReset);
        session.prependOlderMessages({makeMessage(5, "five"), makeMessage(6, "six")});
        model.finishLoadOlder(false);

        QCOMPARE(resetSpy.count(), 0);
        QCOMPARE(insertSpy.count(), 1);
        // Older rows land at END indices (highest index = oldest).
        QCOMPARE(insertSpy.first().at(1).toInt(), 2);
        QCOMPARE(insertSpy.first().at(2).toInt(), 3);
        QCOMPARE(model.rowCount(), 4);
        QCOMPARE(model.data(model.index(0), ChatMessagesModel::MessageIdRole).toLongLong(),
                 qint64(11));
        QCOMPARE(model.data(model.index(3), ChatMessagesModel::MessageIdRole).toLongLong(),
                 qint64(5));
        QVERIFY(!model.isLoadingOlder());
        QVERIFY(!model.hasMore());
        // hasMore == false blocks further requests.
        model.loadOlder();
        QCOMPARE(requestSpy.count(), 1);
    }

    void testToolPartsExposedInPartsRole() {
        ChatSession session(QStringLiteral("c1"));
        ChatMessagesModel model(&session);
        const ChatMessage msg = ChatMessage::fromJson(QJsonDocument::fromJson(R"({
            "id": 1, "chat_id": "c1", "role": "assistant",
            "content": [
                {"type": "tool-call", "tool_call_id": "tc1", "tool_name": "execute",
                 "args": {"command": "ls", "model_intent": "list files"},
                 "created_at": "2026-07-01T10:00:00Z"},
                {"type": "tool-result", "tool_call_id": "tc1", "tool_name": "execute",
                 "result": "ok", "created_at": "2026-07-01T10:00:02Z"}
            ]})")
                                                          .object());
        session.setInitialMessages({msg});

        const QVariantList parts =
            model.data(model.index(0), ChatMessagesModel::PartsRole).toList();
        QCOMPARE(parts.size(), 2);
        const QVariantMap call = parts[0].toMap();
        QCOMPARE(call.value(QStringLiteral("type")).toString(), QStringLiteral("tool-call"));
        QCOMPARE(call.value(QStringLiteral("toolName")).toString(), QStringLiteral("execute"));
        QCOMPARE(call.value(QStringLiteral("modelIntent")).toString(),
                 QStringLiteral("list files"));
        QVERIFY(call.value(QStringLiteral("argsJson"))
                    .toString()
                    .contains(QStringLiteral("\"command\":\"ls\"")));
        const QVariantMap result = parts[1].toMap();
        QCOMPARE(result.value(QStringLiteral("resultText")).toString(), QStringLiteral("ok"));
        // Tool duration pairs the result with its call by tool_call_id.
        QCOMPARE(result.value(QStringLiteral("durationMs")).toLongLong(), qint64(2000));
    }

    void testHasNewerUserMessage() {
        ChatSession session(QStringLiteral("c1"));
        ChatMessagesModel model(&session);
        // Rows (newest-first): 0 = user "answer", 1 = assistant "question",
        // 2 = user "prompt".
        const ChatMessage assistant = ChatMessage::fromJson(QJsonDocument::fromJson(R"({
            "id": 2, "chat_id": "c1", "role": "assistant",
            "content": [{"type": "text", "text": "question"}]})")
                                                                .object());
        session.setInitialMessages({makeMessage(1, "prompt"), assistant, makeMessage(3, "answer")});
        QCOMPARE(model.rowCount(), 3);

        // The assistant row at index 1 has a newer user message (row 0).
        QVERIFY(model.hasNewerUserMessage(1));
        // The oldest row sees both newer rows, one of which is a user row.
        QVERIFY(model.hasNewerUserMessage(2));
        // The newest row has nothing newer; out-of-range rows are false.
        QVERIFY(!model.hasNewerUserMessage(0));
        QVERIFY(!model.hasNewerUserMessage(-1));
        QVERIFY(!model.hasNewerUserMessage(99));
    }

    void testHasNewerUserMessageOnlyAssistantAbove() {
        ChatSession session(QStringLiteral("c1"));
        ChatMessagesModel model(&session);
        const ChatMessage a1 = ChatMessage::fromJson(QJsonDocument::fromJson(R"({
            "id": 2, "chat_id": "c1", "role": "assistant",
            "content": [{"type": "text", "text": "one"}]})")
                                                         .object());
        const ChatMessage a2 = ChatMessage::fromJson(QJsonDocument::fromJson(R"({
            "id": 3, "chat_id": "c1", "role": "assistant",
            "content": [{"type": "text", "text": "two"}]})")
                                                         .object());
        session.setInitialMessages({makeMessage(1, "prompt"), a1, a2});
        // Rows (newest-first): 0 and 1 are assistant messages; the user
        // message itself sits at row 2, so no row has a newer user message.
        QVERIFY(!model.hasNewerUserMessage(1));
        QVERIFY(!model.hasNewerUserMessage(2));
    }

    // -----------------------------------------------------------------------
    // Regressions from live dev.coder.com payloads
    // -----------------------------------------------------------------------

    // Regression: sub-agent rows must inherit the parent's time-group
    // section. On the live list, chat 0cd036c5 (updated today) has children
    // last updated the previous day; grouping children by their own
    // updated_at splits the "Today" section with interleaved "Yesterday"
    // headers, which reads as a mis-ordered list.
    void testSubagentInheritsParentTimeGroup() {
        const QDateTime now = QDateTime::currentDateTimeUtc();
        Chat parent = makeChat(QStringLiteral("parent"), QStringLiteral("Parent"), now);
        Chat child = makeChat(QStringLiteral("child"), QStringLiteral("Child"), now.addDays(-2));
        child.parentChatId = QStringLiteral("parent");
        parent.children = {child};

        ChatListModel m;
        m.setChats({parent});
        QCOMPARE(m.rowCount(), 2);
        QCOMPARE(m.data(m.index(0), ChatListModel::TimeGroupRole).toString(),
                 QStringLiteral("Today"));
        // The child row two days old still belongs to the parent's section.
        QVERIFY(m.data(m.index(1), ChatListModel::IsSubagentRole).toBool());
        QCOMPARE(m.data(m.index(1), ChatListModel::TimeGroupRole).toString(),
                 QStringLiteral("Today"));
    }

    void testSubagentInheritsPinnedSection() {
        const QDateTime now = QDateTime::currentDateTimeUtc();
        Chat parent = makeChat(QStringLiteral("parent"), QStringLiteral("Parent"), now,
                               /*pinOrder=*/1);
        Chat child = makeChat(QStringLiteral("child"), QStringLiteral("Child"), now.addDays(-2));
        child.parentChatId = QStringLiteral("parent");
        parent.children = {child};

        ChatListModel m;
        m.setChats({parent});
        QCOMPARE(m.rowCount(), 2);
        QCOMPARE(m.data(m.index(0), ChatListModel::TimeGroupRole).toString(),
                 QStringLiteral("Pinned"));
        QCOMPARE(m.data(m.index(1), ChatListModel::TimeGroupRole).toString(),
                 QStringLiteral("Pinned"));
    }

    // Regression: a durable message replayed by the stream with an id OLDER
    // than the loaded page must land at the oldest row end, keeping the
    // newest-first row order intact.
    void testOutOfOrderDurableInsertKeepsNewestFirstRows() {
        ChatSession session(QStringLiteral("c1"));
        ChatMessagesModel model(&session);
        session.setInitialMessages(
            {makeMessage(5158447, "page old"), makeMessage(5164185, "page new")});
        QCOMPARE(model.rowCount(), 2);

        // Stream replays an older durable message after the page landed.
        session.applyEvent(messageEvent(QStringLiteral("c1"), 5150000, "replayed"));

        QCOMPARE(model.rowCount(), 3);
        QCOMPARE(model.data(model.index(0), ChatMessagesModel::MessageIdRole).toLongLong(),
                 qint64(5164185));
        QCOMPARE(model.data(model.index(1), ChatMessagesModel::MessageIdRole).toLongLong(),
                 qint64(5158447));
        QCOMPARE(model.data(model.index(2), ChatMessagesModel::MessageIdRole).toLongLong(),
                 qint64(5150000));
    }
};

QTEST_MAIN(TestChatModels)
#include "tst_chatmodels.moc"
