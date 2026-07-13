// ChatLayoutTest: offscreen layout-contract harness for AgentChatPage.qml.
//
// Loads the real AgentChatPage from the CoderDesktop QML module (via the
// filesystem module dir generated in the build tree) against fully mocked
// context properties, then:
//   1. asserts the vertical layout contract (the timeline must own at least
//      75% of a 480x700 window and 85% of a 1280x800 window when the
//      composer is empty and no callout/queue is active), and
//   2. optionally writes window grabs to $CHAT_LAYOUT_SHOT_DIR as
//      "<prefix>-<scenario>-<WxH>.png" (prefix from
//      $CHAT_LAYOUT_SHOT_PREFIX, default "after") for visual inspection.
//
// Test-only target: registered as ctest "ChatLayoutTest", never installed.

#include <QDateTime>
#include <QDir>
#include <QGuiApplication>
#include <QImage>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQmlPropertyMap>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QtTest/QtTest>

namespace {

QVariantMap part(const QString& type, const QVariantMap& extra = {}) {
    QVariantMap p{{"type", type},      {"text", ""},     {"durationMs", 0},  {"toolName", ""},
                  {"modelIntent", ""}, {"argsJson", ""}, {"resultText", ""}, {"isError", false},
                  {"title", ""},       {"url", ""},      {"fileId", ""},     {"skillName", ""}};
    for (auto it = extra.begin(); it != extra.end(); ++it) p.insert(it.key(), it.value());
    return p;
}

QVariantMap textPart(const QString& text) {
    return part("text", {{"text", text}});
}

QVariantMap toolPart(const QString& name, const QString& intent, const QString& args,
                     const QString& result, bool isError = false) {
    return part("tool-call", {{"toolName", name},
                              {"modelIntent", intent},
                              {"argsJson", args},
                              {"resultText", result},
                              {"isError", isError},
                              {"durationMs", 1200}});
}

}  // namespace

// Mock of ChatMessagesModel: fixed roles + the hasMore/loadingOlder/count
// surface AgentChatPage's timeline binds to.
class MockMessagesModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(bool hasMore READ hasMore NOTIFY hasMoreChanged)
    Q_PROPERTY(bool loadingOlder READ loadingOlder NOTIFY loadingOlderChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)
public:
    explicit MockMessagesModel(QObject* parent = nullptr) : QAbstractListModel(parent) {}
    enum Roles { RoleRole = Qt::UserRole + 1, PartsRole, StreamingRole };

    void setMessages(const QVariantList& messages) {
        beginResetModel();
        m_messages = messages;
        endResetModel();
        emit countChanged();
    }
    [[nodiscard]] int rowCount(const QModelIndex& = {}) const override {
        return static_cast<int>(m_messages.size());
    }
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override {
        if (!index.isValid() || index.row() >= m_messages.size()) return {};
        const QVariantMap m = m_messages.at(index.row()).toMap();
        if (role == RoleRole) return m.value("role");
        if (role == PartsRole) return m.value("parts");
        if (role == StreamingRole) return m.value("isStreaming", false);
        return {};
    }
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override {
        return {{RoleRole, "role"}, {PartsRole, "parts"}, {StreamingRole, "isStreaming"}};
    }
    [[nodiscard]] bool hasMore() const { return false; }
    [[nodiscard]] bool loadingOlder() const { return false; }
    [[nodiscard]] int count() const { return rowCount(); }
    Q_INVOKABLE bool hasNewerUserMessage(int) const { return true; }
signals:
    void hasMoreChanged();
    void loadingOlderChanged();
    void countChanged();

private:
    QVariantList m_messages;
};

// Mock of ChatController with the full property/invokable surface used by
// AgentChatPage.qml and its agents/ children. All mutators are no-ops.
class MockChat : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString chatId MEMBER m_chatId NOTIFY changed)
    Q_PROPERTY(QString title MEMBER m_title NOTIFY chatInfoChanged)
    Q_PROPERTY(QString statusString MEMBER m_statusString NOTIFY changed)
    Q_PROPERTY(QString parentChatId MEMBER m_parentChatId NOTIFY chatInfoChanged)
    Q_PROPERTY(QString workspaceId MEMBER m_workspaceId NOTIFY chatInfoChanged)
    Q_PROPERTY(bool archived MEMBER m_archived NOTIFY chatInfoChanged)
    Q_PROPERTY(bool planMode MEMBER m_planMode NOTIFY chatInfoChanged)
    Q_PROPERTY(int connectionState MEMBER m_connectionState NOTIFY changed)
    Q_PROPERTY(QObject* messagesModel READ messagesModel NOTIFY changed)
    Q_PROPERTY(QString draft MEMBER m_draft NOTIFY changed)
    Q_PROPERTY(QStringList prompts MEMBER m_prompts NOTIFY changed)
    Q_PROPERTY(int queuedCount MEMBER m_queuedCount NOTIFY changed)
    Q_PROPERTY(QVariantList queuedMessages MEMBER m_queuedMessages NOTIFY changed)
    Q_PROPERTY(bool hasError MEMBER m_hasError NOTIFY changed)
    Q_PROPERTY(QString errorMessage MEMBER m_errorMessage NOTIFY changed)
    Q_PROPERTY(QString errorKind MEMBER m_errorKind NOTIFY changed)
    Q_PROPERTY(bool errorRetryable MEMBER m_errorRetryable NOTIFY changed)
    Q_PROPERTY(bool hasRetry MEMBER m_hasRetry NOTIFY changed)
    Q_PROPERTY(int retryAttempt MEMBER m_retryAttempt NOTIFY changed)
    Q_PROPERTY(int retryDelayMs MEMBER m_retryDelayMs NOTIFY changed)
    Q_PROPERTY(bool hasActionRequired MEMBER m_hasActionRequired NOTIFY changed)
    Q_PROPERTY(QVariantList actionToolCalls MEMBER m_actionToolCalls NOTIFY changed)
    Q_PROPERTY(bool hasUsageLimit MEMBER m_hasUsageLimit NOTIFY changed)
    Q_PROPERTY(double usageSpentMicros MEMBER m_usageSpentMicros NOTIFY changed)
    Q_PROPERTY(double usageLimitMicros MEMBER m_usageLimitMicros NOTIFY changed)
    Q_PROPERTY(QDateTime usageResetsAt MEMBER m_usageResetsAt NOTIFY changed)
public:
    explicit MockChat(QObject* parent = nullptr) : QObject(parent) {
        m_messagesModel = new MockMessagesModel(this);
    }
    [[nodiscard]] QObject* messagesModel() const { return m_messagesModel; }
    [[nodiscard]] MockMessagesModel* mockModel() const { return m_messagesModel; }

    Q_INVOKABLE void rename(const QString&) {}
    Q_INVOKABLE void reconnect() {}
    Q_INVOKABLE void setArchived(bool) {}
    Q_INVOKABLE void regenerateTitle() {}
    Q_INVOKABLE void loadOlder() {}
    Q_INVOKABLE void interrupt() {}
    Q_INVOKABLE void sendMessage(const QString&) {}
    Q_INVOKABLE void sendMessageWithOptions(const QString&, const QVariantList&, const QString&,
                                            const QVariantMap&) {}
    Q_INVOKABLE void setPlanModeEnabled(bool) {}
    Q_INVOKABLE void promoteQueued(const QString&) {}
    Q_INVOKABLE void deleteQueued(const QString&) {}
    Q_INVOKABLE void refreshMessages() {}
    Q_INVOKABLE void clearUsageLimit() {}
    Q_INVOKABLE void implementPlan() {}
    Q_INVOKABLE void submitToolResult(const QString&, const QString&, bool) {}
    [[nodiscard]] Q_INVOKABLE QVariantList parseAskUserQuestions(const QString&) const {
        return {};
    }
    [[nodiscard]] Q_INVOKABLE QVariantList parsePlanSteps(const QString&) const { return {}; }
    [[nodiscard]] Q_INVOKABLE QVariantMap parseToolSchema(const QString&) const {
        return {{"supported", false}, {"fields", QVariantList{}}};
    }
    [[nodiscard]] Q_INVOKABLE QString formatAskUserAnswers(const QVariantList&,
                                                           const QVariantMap&) const {
        return {};
    }

    QString m_chatId, m_title = "Redesign the chat layout", m_statusString = "running";
    QString m_parentChatId, m_workspaceId = "ws-1", m_draft;
    QString m_errorMessage, m_errorKind;
    bool m_archived = false, m_planMode = false;
    int m_connectionState = 2;
    QStringList m_prompts;
    int m_queuedCount = 0;
    QVariantList m_queuedMessages;
    bool m_hasError = false, m_errorRetryable = false, m_hasRetry = false;
    int m_retryAttempt = 0, m_retryDelayMs = 0;
    bool m_hasActionRequired = false;
    QVariantList m_actionToolCalls;
    bool m_hasUsageLimit = false;
    double m_usageSpentMicros = 0, m_usageLimitMicros = 0;
    QDateTime m_usageResetsAt;

signals:
    void changed();
    void chatInfoChanged();
    void messageSent();

private:
    MockMessagesModel* m_messagesModel = nullptr;  // owned by this (Qt parent)
};

// Scenario knobs applied to the MockChat that openChat() hands out.
struct Scenario {
    int subagentCount = 13;
    bool queued = false;
    bool actionRequired = false;
    bool error = false;
    QString draft;
};

class MockAgentsController : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool available READ available NOTIFY changed)
    Q_PROPERTY(QString focusedChatId MEMBER m_focusedChatId NOTIFY changed)
    Q_PROPERTY(QString sendShortcut READ sendShortcut NOTIFY changed)
    Q_PROPERTY(QVariantList modelConfigs READ modelConfigs NOTIFY changed)
    Q_PROPERTY(QVariantList mcpServers READ mcpServers NOTIFY changed)
public:
    explicit MockAgentsController(QObject* parent = nullptr) : QObject(parent) {}
    [[nodiscard]] bool available() const { return true; }
    [[nodiscard]] QString sendShortcut() const { return "enter"; }
    [[nodiscard]] QVariantList modelConfigs() const { return {}; }
    [[nodiscard]] QVariantList mcpServers() const { return {}; }

    QString m_focusedChatId;

    Q_INVOKABLE QObject* openChat(const QString& chatId) {
        // Returned without a parent: QML takes JavaScript ownership so the
        // page's chat.destroy() calls work exactly like production.
        auto* chat = new MockChat();
        chat->m_chatId = chatId;
        chat->m_draft = scenario.draft;
        chat->mockModel()->setMessages(buildMessages());
        if (scenario.queued) {
            chat->m_queuedCount = 2;
            chat->m_queuedMessages = {
                QVariantMap{{"queuedId", "q1"}, {"text", "Also add unit tests for the parser"}},
                QVariantMap{{"queuedId", "q2"}, {"text", "Then update the changelog"}}};
        }
        if (scenario.actionRequired) {
            chat->m_hasActionRequired = true;
            chat->m_actionToolCalls = {
                QVariantMap{{"toolCallId", "tc-1"},
                            {"toolName", "request_credentials"},
                            {"argsJson", "{\"provider\":\"github\",\"scopes\":[\"repo\"]}"}}};
        }
        if (scenario.error) {
            chat->m_hasError = true;
            chat->m_errorKind = "overloaded";
            chat->m_errorMessage = "upstream provider returned 529";
            chat->m_errorRetryable = true;
        }
        return chat;
    }

    [[nodiscard]] Q_INVOKABLE QVariantList subagentsOf(const QString&) const {
        static const QStringList statuses{"running", "completed", "error",
                                          "pending", "waiting",   "requires_action"};
        QVariantList out;
        for (int i = 0; i < scenario.subagentCount; ++i) {
            out.append(QVariantMap{{"id", QString("sub-%1").arg(i)},
                                   {"title", QString("Sub-agent task %1: refactor module").arg(i)},
                                   {"statusString", statuses.at(i % statuses.size())}});
        }
        return out;
    }
    Q_INVOKABLE void uploadAttachment(const QUrl&) {}

    Scenario scenario;

    static QVariantList buildMessages() {
        // Index 0 = newest (BottomToTop timeline). ~15 mixed messages.
        QVariantList msgs;
        auto add = [&msgs](const QString& role, const QVariantList& parts, bool streaming = false) {
            msgs.append(QVariantMap{{"role", role}, {"parts", parts}, {"isStreaming", streaming}});
        };
        add("assistant", {textPart("Working on the layout refactor now, one moment...")}, true);
        add("user", {textPart("Please make the timeline take all remaining space.")});
        add("assistant", {toolPart("edit_files", "Adjusting the page column",
                                   "{\"path\":\"AgentChatPage.qml\"}", "ok")});
        add("assistant", {textPart("I found three sections reserving fixed height:\n\n"
                                   "1. the sub-agent strip\n2. the callout stack\n"
                                   "3. the composer chip row")});
        add("user", {textPart("What is eating the vertical space?")});
        add("assistant", {part("reasoning", {{"text",
                                              "Measuring each section against the window "
                                              "height to find the fixed reservations."},
                                             {"durationMs", 5400}})});
        add("assistant",
            {toolPart("execute", "Running the layout probe",
                      "{\"command\":\"ctest -R ChatLayoutTest\"}", "1/1 tests passed")});
        add("assistant",
            {toolPart("read_file", "Reading the chat page",
                      "{\"path\":\"app/qml/AgentChatPage.qml\"}",
                      "776 lines: header, timeline, subagent strip, callouts, composer")});
        add("user", {textPart("Start with a diagnosis, then redesign.")});
        add("assistant", {textPart("Here is the plan:\n\n```qml\nColumnLayout {\n    Header {}\n"
                                   "    Timeline { Layout.fillHeight: true }\n    Dock {}\n}\n"
                                   "```\n\nThe timeline becomes the only fill item.")});
        add("assistant", {toolPart("web_search", "Checking Qt Drawer docs",
                                   "{\"query\":\"QtQuick Drawer edge\"}",
                                   "Drawer opens from any window edge", true)});
        add("assistant", {part("reasoning", {{"text",
                                              "The strip reserves height even when "
                                              "collapsed; a drawer costs zero."},
                                             {"durationMs", 2100}})});
        add("user", {textPart("The chat window is half reserved for sub-agents.")});
        add("assistant", {textPart("**Welcome!** I can help with the layout. Markdown works: "
                                   "*emphasis*, `code`, and\n\n- lists\n- too")});
        add("user", {textPart("Hi, can you redesign the chat view?")});
        return msgs;
    }

signals:
    void changed();
    void chatUpserted(QVariant chat);
    void chatsReset(QVariant chats);
};

class MockWorkspaceModel : public QObject {
    Q_OBJECT
public:
    explicit MockWorkspaceModel(QObject* parent = nullptr) : QObject(parent) {}
    [[nodiscard]] Q_INVOKABLE QVariantMap infoForId(const QString&) const {
        return {{"found", true}, {"name", "dev-workspace"}, {"statusString", "Running"}};
    }
signals:
    void countChanged();
    void dataChanged();
};

class ChatLayoutTest : public QObject {
    Q_OBJECT

    struct Loaded {
        std::unique_ptr<QQmlEngine> engine;
        QQuickWindow* window = nullptr;  // owned by engine root object
        QQuickItem* page = nullptr;      // owned by window
        // Set when the harness fails to load because a QML runtime module
        // (QtQuick, Controls, WorkerScript, ...) is not installed on the
        // host. Lets the test skip instead of fail on minimal CI images.
        bool missingQmlModules = false;
    };

    // The generated CoderDesktop qmldir carries "prefer :/CoderDesktop/",
    // which redirects loads into resources that only exist inside the
    // coder-desktop executable. Shadow the module into a temp dir with the
    // prefer line stripped so this test loads the same QML from disk.
    static QString shadowImportDir() {
        static QString cached;
        if (!cached.isEmpty()) return cached;
        static QTemporaryDir tmp;
        const QString srcModule = QStringLiteral(CHAT_LAYOUT_IMPORT_DIR "/CoderDesktop");
        const QString dstModule = tmp.path() + "/CoderDesktop";
        QDir().mkpath(dstModule);
        QFile::link(srcModule + "/qml", dstModule + "/qml");
        QFile in(srcModule + "/qmldir");
        QFile out(dstModule + "/qmldir");
        if (in.open(QIODevice::ReadOnly | QIODevice::Text) &&
            out.open(QIODevice::WriteOnly | QIODevice::Text)) {
            while (!in.atEnd()) {
                const QByteArray line = in.readLine();
                if (line.startsWith("prefer ")) continue;
                out.write(line);
            }
        }
        cached = tmp.path();
        return cached;
    }

    Loaded load(const Scenario& scenario, int width, int height) {
        Loaded l;
        l.engine = std::make_unique<QQmlEngine>();
        l.engine->addImportPath(shadowImportDir());
        auto* agents = new MockAgentsController(l.engine.get());
        agents->scenario = scenario;
        auto* workspaces = new MockWorkspaceModel(l.engine.get());
        auto* settings = new QQmlPropertyMap(l.engine.get());
        l.engine->rootContext()->setContextProperty("agentsController", agents);
        l.engine->rootContext()->setContextProperty("workspaceModel", workspaces);
        l.engine->rootContext()->setContextProperty("settingsManager", settings);

        QQmlComponent component(l.engine.get(), QUrl::fromLocalFile(CHAT_LAYOUT_HARNESS));
        QObject* root = component.create();
        if (!root) {
            for (const auto& e : component.errors()) {
                qWarning() << e;
                if (e.description().contains(QStringLiteral("is not installed")))
                    l.missingQmlModules = true;
            }
        }
        l.window = qobject_cast<QQuickWindow*>(root);
        if (!l.window) return l;
        l.window->setWidth(width);
        l.window->setHeight(height);
        l.page = l.window->findChild<QQuickItem*>("chatPage");
        [[maybe_unused]] const bool exposed = QTest::qWaitForWindowExposed(l.window);
        QTest::qWait(400);
        return l;
    }

    // Prints one line per direct child of the page's main ColumnLayout so
    // before/after section heights can be compared from the test log.
    static void dumpSections(QQuickItem* page, const QString& label) {
        if (!page) return;
        QQuickItem* column = nullptr;
        const auto kids = page->childItems();
        for (QQuickItem* c : kids) {
            if (QByteArrayView(c->metaObject()->className()).startsWith("QQuickColumnLayout")) {
                column = c;
                break;
            }
        }
        if (!column) {
            qInfo() << label << "no ColumnLayout found";
            return;
        }
        qInfo().noquote() << QString("[%1] window %2x%3, column h=%4")
                                 .arg(label)
                                 .arg(page->width())
                                 .arg(page->height())
                                 .arg(column->height());
        const auto sections = column->childItems();
        for (QQuickItem* s : sections) {
            qInfo().noquote()
                << QString("  %1 objectName='%2' y=%3 h=%4 visible=%5")
                       .arg(QString::fromLatin1(s->metaObject()->className()).section('_', 0, 0))
                       .arg(s->objectName())
                       .arg(s->y())
                       .arg(s->height())
                       .arg(s->isVisible());
        }
    }

    // The timeline is the tallest ListView on the page (works both before
    // and after the redesign, with or without objectNames).
    static QQuickItem* findTimeline(QQuickItem* page) {
        if (!page) return nullptr;
        if (auto* named = page->findChild<QQuickItem*>("timeline")) return named;
        QQuickItem* best = nullptr;
        const auto all = page->findChildren<QQuickItem*>();
        for (QQuickItem* c : all) {
            if (!QByteArrayView(c->metaObject()->className()).contains("ListView")) continue;
            if (!c->isVisible()) continue;
            if (!best || c->height() > best->height()) best = c;
        }
        return best;
    }

    void grab(const Loaded& l, const QString& scenarioName) {
        const QString dir = qEnvironmentVariable("CHAT_LAYOUT_SHOT_DIR");
        if (dir.isEmpty() || !l.window) return;
        QDir().mkpath(dir);
        const QString prefix = qEnvironmentVariable("CHAT_LAYOUT_SHOT_PREFIX", "after");
        const QImage img = l.window->grabWindow();
        const QString path = QString("%1/%2-%3-%4x%5.png")
                                 .arg(dir, prefix, scenarioName)
                                 .arg(l.window->width())
                                 .arg(l.window->height());
        if (!img.save(path))
            qWarning() << "failed to save" << path;
        else
            qInfo().noquote() << "saved" << path;
    }

private slots:
    void heightContract_data() {
        QTest::addColumn<int>("width");
        QTest::addColumn<int>("height");
        QTest::addColumn<double>("minRatio");
        QTest::newRow("480x700") << 480 << 700 << 0.75;
        QTest::newRow("1280x800") << 1280 << 800 << 0.85;
    }

    // Contract: with an empty composer and no callouts/queue, the timeline
    // owns >= 75% of the window at 480x700 and >= 85% at 1280x800, even
    // with 13 sub-agent chats present.
    void heightContract() {
        QFETCH(int, width);
        QFETCH(int, height);
        QFETCH(double, minRatio);

        Scenario clean;  // 13 sub-agents, nothing queued, no callouts
        Loaded l = load(clean, width, height);
        if (l.missingQmlModules) QSKIP("QML runtime modules are not installed on this host");
        QVERIFY(l.window);
        QVERIFY(l.page);
        dumpSections(l.page, QString("contract %1x%2").arg(width).arg(height));

        QQuickItem* timeline = findTimeline(l.page);
        QVERIFY2(timeline, "timeline ListView not found");
        if (QQuickItem* header = l.page->findChild<QQuickItem*>("chatHeader"))
            QVERIFY2(header->height() <= 44.0,
                     qPrintable(QString("header is %1px tall, contract requires <= 44px")
                                    .arg(header->height())));
        const double ratio = timeline->height() / double(height);
        qInfo().noquote() << QString("timeline h=%1 window h=%2 ratio=%3 (min %4)")
                                 .arg(timeline->height())
                                 .arg(height)
                                 .arg(ratio, 0, 'f', 3)
                                 .arg(minRatio);
        grab(l, "contract");
        QVERIFY2(ratio >= minRatio,
                 qPrintable(QString("timeline owns %1%% of the window, contract requires >= %2%%")
                                .arg(ratio * 100, 0, 'f', 1)
                                .arg(minRatio * 100, 0, 'f', 0)));
    }

    void screenshots_data() {
        QTest::addColumn<QString>("name");
        QTest::addColumn<Scenario>("scenario");
        QTest::addColumn<bool>("openDrawer");
        Scenario base;
        base.queued = true;
        QTest::newRow("base") << "base" << base << false;
        Scenario action;
        action.actionRequired = true;
        action.error = true;
        QTest::newRow("action") << "action" << action << false;
        Scenario longtext;
        longtext.draft = QString("This is a long draft message.\n").repeated(14);
        QTest::newRow("longtext") << "longtext" << longtext << false;
        Scenario drawer;
        QTest::newRow("drawer") << "drawer" << drawer << true;
    }

    void screenshots() {
        if (qEnvironmentVariable("CHAT_LAYOUT_SHOT_DIR").isEmpty())
            QSKIP("CHAT_LAYOUT_SHOT_DIR not set; screenshot pass skipped");
        QFETCH(QString, name);
        QFETCH(Scenario, scenario);
        QFETCH(bool, openDrawer);
        const QList<QSize> sizes{{480, 700}, {1280, 800}};
        for (const QSize& size : sizes) {
            Loaded l = load(scenario, size.width(), size.height());
            if (l.missingQmlModules) QSKIP("QML runtime modules are not installed on this host");
            QVERIFY(l.window);
            if (openDrawer) {
                if (QObject* drawer = l.window->findChild<QObject*>("subagentDrawer")) {
                    QMetaObject::invokeMethod(drawer, "open");
                    QTest::qWait(500);
                } else {
                    qInfo() << "subagentDrawer not present; skipping drawer open for" << name;
                }
            }
            dumpSections(l.page,
                         QString("%1 %2x%3").arg(name).arg(size.width()).arg(size.height()));
            grab(l, name);
        }
    }
};

Q_DECLARE_METATYPE(Scenario)

int main(int argc, char** argv) {
    if (qEnvironmentVariable("QT_QPA_PLATFORM").isEmpty()) qputenv("QT_QPA_PLATFORM", "offscreen");
    // Software rendering: grabWindow works headless without GL.
    QQuickWindow::setGraphicsApi(QSGRendererInterface::Software);
    QGuiApplication app(argc, argv);
    QQuickStyle::setStyle(QStringLiteral("Material"));
    ChatLayoutTest tc;
    return QTest::qExec(&tc, argc, argv);
}

#include "tst_chatlayout.moc"
