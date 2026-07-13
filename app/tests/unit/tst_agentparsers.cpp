#include <QJsonDocument>
#include <QJsonObject>
#include <QtTest>

#include "agents/AskUserQuestionParser.h"
#include "agents/DiffParser.h"
#include "agents/JsonSchemaParser.h"
#include "agents/PlanStepParser.h"

// Unit tests for the QML-facing agent parsers: PlanStepParser (semantics
// ported from Android PlanStepParser.kt), JsonSchemaParser (dynamic tool
// runner forms), and DiffParser (ported from Android DiffParser.kt).
class TestAgentParsers : public QObject {
    Q_OBJECT

    static QJsonObject parseJson(const char* json) {
        return QJsonDocument::fromJson(json).object();
    }

private slots:
    // -- PlanStepParser -----------------------------------------------------

    void planEmptyInput() {
        QVERIFY(PlanStepParser::parse(QString()).isEmpty());
        QVERIFY(PlanStepParser::parse(QStringLiteral("   \n\t")).isEmpty());
    }

    void planHeadings() {
        const auto steps = PlanStepParser::parse(
            QStringLiteral("## Step 1: Foo\n\nbody one\n\n## Step 2: Bar\nbody two"));
        QCOMPARE(steps.size(), 2);
        QCOMPARE(steps.at(0).index, 1);
        QCOMPARE(steps.at(0).title, QStringLiteral("Foo"));
        QCOMPARE(steps.at(0).body, QStringLiteral("body one"));
        QCOMPARE(steps.at(1).title, QStringLiteral("Bar"));
        QCOMPARE(steps.at(1).body, QStringLiteral("body two"));
        QVERIFY(!steps.at(0).checked);
    }

    void planHeadingsCaseInsensitiveAndLevels() {
        const auto steps =
            PlanStepParser::parse(QStringLiteral("### STEP 1 - Alpha\n#### 2. Beta"));
        QCOMPARE(steps.size(), 2);
        QCOMPARE(steps.at(0).title, QStringLiteral("Alpha"));
        QCOMPARE(steps.at(1).title, QStringLiteral("Beta"));
    }

    void planOrderedList() {
        const auto steps = PlanStepParser::parse(QStringLiteral("1. Plan it\n2) Build it"));
        QCOMPARE(steps.size(), 2);
        QCOMPARE(steps.at(0).title, QStringLiteral("Plan it"));
        QCOMPARE(steps.at(1).title, QStringLiteral("Build it"));
        QCOMPARE(steps.at(1).index, 2);
    }

    void planTaskListChecked() {
        const auto steps =
            PlanStepParser::parse(QStringLiteral("- [x] Setup\n- [ ] Deploy\n- [X] Test"));
        QCOMPARE(steps.size(), 3);
        QVERIFY(steps.at(0).checked);
        QVERIFY(!steps.at(1).checked);
        QVERIFY(steps.at(2).checked);
        QCOMPARE(steps.at(1).title, QStringLiteral("Deploy"));
    }

    void planHeadingsWinOverLists() {
        // Patterns are never mixed: headings take priority even when an
        // ordered list is also present.
        const auto steps = PlanStepParser::parse(
            QStringLiteral("## Step 1: Top\n1. inner a\n2. inner b\n## Step 2: Next"));
        QCOMPARE(steps.size(), 2);
        QCOMPARE(steps.at(0).title, QStringLiteral("Top"));
        QVERIFY(steps.at(0).body.contains(QStringLiteral("inner a")));
    }

    void planCapAt100() {
        QString text;
        for (int i = 1; i <= 150; ++i) text += QStringLiteral("%1. Item %1\n").arg(i);
        QCOMPARE(PlanStepParser::parse(text).size(), 100);
    }

    void planVariantProjection() {
        const auto list =
            PlanStepParser::toVariantList(PlanStepParser::parse(QStringLiteral("- [x] Done")));
        QCOMPARE(list.size(), 1);
        const QVariantMap m = list.first().toMap();
        QCOMPARE(m.value(QStringLiteral("title")).toString(), QStringLiteral("Done"));
        QCOMPARE(m.value(QStringLiteral("checked")).toBool(), true);
        QCOMPARE(m.value(QStringLiteral("index")).toInt(), 1);
    }

    // -- JsonSchemaParser ---------------------------------------------------

    void schemaScalarFields() {
        bool ok = false;
        const QVariantList fields = JsonSchemaParser::parse(parseJson(R"({
            "type": "object",
            "required": ["name"],
            "properties": {
                "name": {"type": "string", "description": "the name"},
                "count": {"type": "integer"},
                "ratio": {"type": "number"},
                "enabled": {"type": "boolean"},
                "mode": {"type": "string", "enum": ["fast", "slow"]}
            }
        })"),
                                                            &ok);
        QVERIFY(ok);
        QCOMPARE(fields.size(), 5);
        QHash<QString, QVariantMap> byKey;
        for (const QVariant& v : fields) {
            const QVariantMap m = v.toMap();
            byKey.insert(m.value(QStringLiteral("key")).toString(), m);
        }
        QCOMPARE(byKey.value(QStringLiteral("name")).value(QStringLiteral("type")).toString(),
                 QStringLiteral("string"));
        QVERIFY(byKey.value(QStringLiteral("name")).value(QStringLiteral("required")).toBool());
        QVERIFY(!byKey.value(QStringLiteral("count")).value(QStringLiteral("required")).toBool());
        QCOMPARE(byKey.value(QStringLiteral("mode")).value(QStringLiteral("type")).toString(),
                 QStringLiteral("enum"));
        QCOMPARE(
            byKey.value(QStringLiteral("mode")).value(QStringLiteral("enumOptions")).toStringList(),
            QStringList({QStringLiteral("fast"), QStringLiteral("slow")}));
    }

    void schemaNullableUnionType() {
        bool ok = false;
        const QVariantList fields = JsonSchemaParser::parse(parseJson(R"({
            "type": "object",
            "properties": {"note": {"type": ["null", "string"]}}
        })"),
                                                            &ok);
        QVERIFY(ok);
        QCOMPARE(fields.first().toMap().value(QStringLiteral("type")).toString(),
                 QStringLiteral("string"));
    }

    void schemaNestedObjectOneLevel() {
        bool ok = false;
        const QVariantList fields = JsonSchemaParser::parse(parseJson(R"({
            "type": "object",
            "properties": {
                "options": {
                    "type": "object",
                    "properties": {"depth": {"type": "integer"}}
                }
            }
        })"),
                                                            &ok);
        QVERIFY(ok);
        const QVariantMap obj = fields.first().toMap();
        QCOMPARE(obj.value(QStringLiteral("type")).toString(), QStringLiteral("object"));
        const QVariantList children = obj.value(QStringLiteral("fields")).toList();
        QCOMPARE(children.size(), 1);
        QCOMPARE(children.first().toMap().value(QStringLiteral("key")).toString(),
                 QStringLiteral("depth"));
    }

    void schemaUnsupportedShapes() {
        bool ok = true;
        // Array property types are unsupported.
        QVERIFY(JsonSchemaParser::parse(parseJson(R"({
            "type": "object",
            "properties": {"items": {"type": "array"}}
        })"),
                                        &ok)
                    .isEmpty());
        QVERIFY(!ok);
        // Two levels of object nesting are unsupported.
        ok = true;
        QVERIFY(JsonSchemaParser::parse(parseJson(R"({
            "type": "object",
            "properties": {"a": {"type": "object", "properties":
                {"b": {"type": "object", "properties": {"c": {"type": "string"}}}}}}
        })"),
                                        &ok)
                    .isEmpty());
        QVERIFY(!ok);
        // Non-object top level is unsupported.
        ok = true;
        QVERIFY(JsonSchemaParser::parse(parseJson(R"({"type": "string"})"), &ok).isEmpty());
        QVERIFY(!ok);
        // Empty input is unsupported.
        ok = true;
        QVERIFY(JsonSchemaParser::parse(QJsonObject(), &ok).isEmpty());
        QVERIFY(!ok);
    }

    void schemaMissingTypeWithProperties() {
        bool ok = false;
        const QVariantList fields = JsonSchemaParser::parse(parseJson(R"({
            "properties": {"q": {"type": "string"}}
        })"),
                                                            &ok);
        QVERIFY(ok);
        QCOMPARE(fields.size(), 1);
    }

    // -- DiffParser -----------------------------------------------------------

    void diffEmpty() {
        QVERIFY(DiffParser::parse(QString()).isEmpty());
        QVERIFY(DiffParser::parse(QStringLiteral("  \n")).isEmpty());
    }

    void diffMultiFile() {
        const QString diff = QStringLiteral(
            "diff --git a/foo.txt b/foo.txt\n"
            "index 111..222 100644\n"
            "--- a/foo.txt\n"
            "+++ b/foo.txt\n"
            "@@ -1,2 +1,2 @@\n"
            " context\n"
            "-old line\n"
            "+new line\n"
            "diff --git a/old.txt b/renamed.txt\n"
            "similarity index 90%\n"
            "rename from old.txt\n"
            "rename to renamed.txt\n"
            "@@ -1 +1,2 @@\n"
            " keep\n"
            "+added\n");
        const auto files = DiffParser::parse(diff);
        QCOMPARE(files.size(), 2);
        QCOMPARE(files.at(0).path, QStringLiteral("foo.txt"));
        QVERIFY(files.at(0).oldPath.isEmpty());
        // +++/--- headers are excluded from the counts.
        QCOMPARE(files.at(0).addCount, 1);
        QCOMPARE(files.at(0).delCount, 1);
        QCOMPARE(files.at(1).path, QStringLiteral("renamed.txt"));
        QCOMPARE(files.at(1).oldPath, QStringLiteral("old.txt"));
        QCOMPARE(files.at(1).addCount, 1);
        QCOMPARE(files.at(1).delCount, 0);
    }

    void diffDeletionUsesOldPath() {
        const QString diff = QStringLiteral(
            "diff --git a/gone.txt b/gone.txt\n"
            "deleted file mode 100644\n"
            "--- a/gone.txt\n"
            "+++ /dev/null\n"
            "@@ -1 +0,0 @@\n"
            "-bye\n");
        const auto files = DiffParser::parse(diff);
        QCOMPARE(files.size(), 1);
        // The b/ path in the git header still names the file.
        QCOMPARE(files.first().path, QStringLiteral("gone.txt"));
        QCOMPARE(files.first().delCount, 1);
    }

    void diffWithoutGitHeaders() {
        const auto files = DiffParser::parse(QStringLiteral("--- a\n+++ b\n@@ -1 +1 @@\n-x\n+y\n"));
        QCOMPARE(files.size(), 1);
        QCOMPARE(files.first().path, QStringLiteral("(diff)"));
        QCOMPARE(files.first().addCount, 1);
        QCOMPARE(files.first().delCount, 1);
    }

    void diffVariantLineKinds() {
        const auto files =
            DiffParser::parse(QStringLiteral("diff --git a/f b/f\n"
                                             "index 1..2\n"
                                             "@@ -1 +1 @@\n"
                                             "-a\n"
                                             "+b\n"
                                             " c\n"));
        const QVariantList list = DiffParser::toVariantList(files);
        QCOMPARE(list.size(), 1);
        const QVariantList hunks = list.first().toMap().value(QStringLiteral("hunks")).toList();
        // The trailing newline yields a final empty context line.
        QCOMPARE(hunks.size(), 7);
        const auto kindAt = [&hunks](int i) {
            return hunks.at(i).toMap().value(QStringLiteral("kind")).toString();
        };
        QCOMPARE(kindAt(0), QStringLiteral("meta"));
        QCOMPARE(kindAt(1), QStringLiteral("meta"));
        QCOMPARE(kindAt(2), QStringLiteral("hunk"));
        QCOMPARE(kindAt(3), QStringLiteral("del"));
        QCOMPARE(kindAt(4), QStringLiteral("add"));
        QCOMPARE(kindAt(5), QStringLiteral("context"));
    }

    // -- AskUserQuestionParser ------------------------------------------------

    static QVariantMap optionAnswer(const QString& label, int index) {
        return {{QStringLiteral("kind"), QStringLiteral("option")},
                {QStringLiteral("label"), label},
                {QStringLiteral("optionIndex"), index}};
    }

    static QVariantMap otherAnswer(const QString& text) {
        return {{QStringLiteral("kind"), QStringLiteral("other")}, {QStringLiteral("text"), text}};
    }

    void askUserParseValid() {
        const QString args = QStringLiteral(R"({"questions": [
            {"header": "Scope", "question": "Which scope?",
             "options": [{"label": "Small", "description": "Just the fix"},
                          {"label": "Large", "description": "Refactor too"}]}
        ]})");
        const QVariantList questions = AskUserQuestionParser::parse(args);
        QCOMPARE(questions.size(), 1);
        const QVariantMap q = questions.first().toMap();
        QCOMPARE(q.value(QStringLiteral("header")).toString(), QStringLiteral("Scope"));
        QCOMPARE(q.value(QStringLiteral("question")).toString(), QStringLiteral("Which scope?"));
        const QVariantList options = q.value(QStringLiteral("options")).toList();
        QCOMPARE(options.size(), 2);
        QCOMPARE(options.first().toMap().value(QStringLiteral("label")).toString(),
                 QStringLiteral("Small"));
        QCOMPARE(options.first().toMap().value(QStringLiteral("description")).toString(),
                 QStringLiteral("Just the fix"));
    }

    void askUserParseFiltersModelProvidedOther() {
        // Model-provided "Other" options (any case) are dropped; the UI
        // appends its own free-text Other, matching the web UI.
        const QString args = QStringLiteral(R"({"questions": [
            {"header": "H", "question": "Q",
             "options": [{"label": "A", "description": ""},
                          {"label": " other ", "description": "x"},
                          {"label": "OTHER", "description": "y"}]}
        ]})");
        const QVariantList questions = AskUserQuestionParser::parse(args);
        QCOMPARE(questions.size(), 1);
        const QVariantList options =
            questions.first().toMap().value(QStringLiteral("options")).toList();
        QCOMPARE(options.size(), 1);
        QCOMPARE(options.first().toMap().value(QStringLiteral("label")).toString(),
                 QStringLiteral("A"));
    }

    void askUserParseInvalidOrPartial() {
        // Truncated streaming JSON, wrong shapes, and empty questions all
        // yield an empty list so the caller falls back to the generic card.
        QVERIFY(AskUserQuestionParser::parse(QString()).isEmpty());
        QVERIFY(AskUserQuestionParser::parse(QStringLiteral("{\"questions\": [{\"hea")).isEmpty());
        QVERIFY(AskUserQuestionParser::parse(QStringLiteral("not json")).isEmpty());
        QVERIFY(AskUserQuestionParser::parse(QStringLiteral("{}")).isEmpty());
        QVERIFY(AskUserQuestionParser::parse(QStringLiteral(R"({"questions": []})")).isEmpty());
        QVERIFY(AskUserQuestionParser::parse(QStringLiteral(R"({"questions": "x"})")).isEmpty());
        QVERIFY(AskUserQuestionParser::parse(QStringLiteral(R"({"questions": [1]})")).isEmpty());
        QVERIFY(AskUserQuestionParser::parse(QStringLiteral("[]")).isEmpty());
    }

    void askUserFormatSingleQuestion() {
        const QVariantList questions = AskUserQuestionParser::parse(QStringLiteral(
            R"({"questions": [{"header": "Scope", "question": "Q",
                "options": [{"label": "Small", "description": ""}]}]})"));
        // Single question: just the answer text.
        QCOMPARE(AskUserQuestionParser::formatAnswers(questions, {optionAnswer("Small", 0)}),
                 QStringLiteral("Small"));
        // Free-form answers are prefixed and trimmed.
        QCOMPARE(AskUserQuestionParser::formatAnswers(questions, {otherAnswer("  my answer  ")}),
                 QStringLiteral("Other: my answer"));
    }

    void askUserFormatMultipleQuestions() {
        const QVariantList questions = AskUserQuestionParser::parse(QStringLiteral(
            R"({"questions": [
                {"header": "Scope", "question": "Q1",
                 "options": [{"label": "Small", "description": ""}]},
                {"header": "", "question": "Q2",
                 "options": [{"label": "Yes", "description": ""}]}
            ]})"));
        // Numbered lines with the header, falling back to "Question N".
        QCOMPARE(AskUserQuestionParser::formatAnswers(
                     questions, {optionAnswer("Small", 0), otherAnswer("custom")}),
                 QStringLiteral("1. Scope: Small\n2. Question 2: Other: custom"));
    }
};

QTEST_GUILESS_MAIN(TestAgentParsers)
#include "tst_agentparsers.moc"
