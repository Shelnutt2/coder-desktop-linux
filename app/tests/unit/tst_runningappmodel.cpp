#include <QSignalSpy>
#include <QTest>

#include "apps/RunningAppModel.h"

/// Unit tests for RunningAppModel.
class TestRunningAppModel : public QObject {
    Q_OBJECT

private slots:
    void testRoleNames();
    void testAddApp();
    void testRemoveApp();
    void testClear();
    void testDataRoles();
    void testRemoveNonexistent();
    void testCountProperty();
};

void TestRunningAppModel::testRoleNames() {
    RunningAppModel model;
    auto roles = model.roleNames();
    QCOMPARE(roles.value(RunningAppModel::PidRole), QByteArray("pid"));
    QCOMPARE(roles.value(RunningAppModel::AppNameRole), QByteArray("appName"));
    QCOMPARE(roles.value(RunningAppModel::CommandRole), QByteArray("command"));
    QCOMPARE(roles.size(), 3);
}

void TestRunningAppModel::testAddApp() {
    RunningAppModel model;
    QCOMPARE(model.rowCount(), 0);

    QSignalSpy countSpy(&model, &RunningAppModel::countChanged);
    model.addApp(1234, QStringLiteral("firefox"), QStringLiteral("firefox --new-window"));
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(countSpy.count(), 1);

    model.addApp(5678, QStringLiteral("code"), QStringLiteral("code ."));
    QCOMPARE(model.rowCount(), 2);
    QCOMPARE(countSpy.count(), 2);
}

void TestRunningAppModel::testRemoveApp() {
    RunningAppModel model;
    model.addApp(100, QStringLiteral("app1"), QStringLiteral("cmd1"));
    model.addApp(200, QStringLiteral("app2"), QStringLiteral("cmd2"));
    QCOMPARE(model.rowCount(), 2);

    QSignalSpy countSpy(&model, &RunningAppModel::countChanged);
    model.removeApp(100);
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(countSpy.count(), 1);

    // Remaining entry should be app2
    auto idx = model.index(0);
    QCOMPARE(model.data(idx, RunningAppModel::PidRole).toInt(), 200);
}

void TestRunningAppModel::testClear() {
    RunningAppModel model;
    model.addApp(1, QStringLiteral("a"), QStringLiteral("a"));
    model.addApp(2, QStringLiteral("b"), QStringLiteral("b"));
    model.addApp(3, QStringLiteral("c"), QStringLiteral("c"));
    QCOMPARE(model.rowCount(), 3);

    QSignalSpy countSpy(&model, &RunningAppModel::countChanged);
    model.clear();
    QCOMPARE(model.rowCount(), 0);
    QCOMPARE(countSpy.count(), 1);

    // Clearing an already-empty model should be a no-op (no signal).
    model.clear();
    QCOMPARE(countSpy.count(), 1);
}

void TestRunningAppModel::testDataRoles() {
    RunningAppModel model;
    model.addApp(42, QStringLiteral("kitty"), QStringLiteral("kitty --single-instance"));

    auto idx = model.index(0);
    QCOMPARE(model.data(idx, RunningAppModel::PidRole).toInt(), 42);
    QCOMPARE(model.data(idx, RunningAppModel::AppNameRole).toString(), QStringLiteral("kitty"));
    QCOMPARE(model.data(idx, RunningAppModel::CommandRole).toString(),
             QStringLiteral("kitty --single-instance"));

    // Invalid role returns empty QVariant.
    QVERIFY(!model.data(idx, Qt::DisplayRole).isValid());

    // Out-of-range index returns empty QVariant.
    auto bad = model.index(99);
    QVERIFY(!model.data(bad, RunningAppModel::PidRole).isValid());
}

void TestRunningAppModel::testRemoveNonexistent() {
    RunningAppModel model;
    model.addApp(1, QStringLiteral("a"), QStringLiteral("a"));

    QSignalSpy countSpy(&model, &RunningAppModel::countChanged);
    // Removing a PID that doesn't exist should be a safe no-op.
    model.removeApp(9999);
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(countSpy.count(), 0);
}

void TestRunningAppModel::testCountProperty() {
    RunningAppModel model;
    QCOMPARE(model.count(), 0);
    QCOMPARE(model.count(), model.rowCount());

    model.addApp(1, QStringLiteral("x"), QStringLiteral("x"));
    QCOMPARE(model.count(), 1);
    QCOMPARE(model.count(), model.rowCount());

    model.addApp(2, QStringLiteral("y"), QStringLiteral("y"));
    QCOMPARE(model.count(), 2);

    model.removeApp(1);
    QCOMPARE(model.count(), 1);

    model.clear();
    QCOMPARE(model.count(), 0);
}

QTEST_MAIN(TestRunningAppModel)
#include "tst_runningappmodel.moc"
