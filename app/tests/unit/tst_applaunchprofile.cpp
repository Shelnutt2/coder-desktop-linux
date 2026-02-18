#include <QTest>

#include "apps/AppLaunchProfile.h"

/// Unit tests for AppLaunchProfile static registry.
class TestAppLaunchProfile : public QObject {
    Q_OBJECT

private slots:
    void testKnownBrowser();
    void testKnownElectronIde();
    void testKnownJetbrainsIde();
    void testKnownTerminal();
    void testUnknownApp();
    void testHasProfile();
    void testKnownAppIds();
};

void TestAppLaunchProfile::testKnownBrowser() {
    // Browsers: need multiprocess + shared memory + home rw.
    auto p = AppLaunchProfile::profileFor(QStringLiteral("firefox"));
    QCOMPARE(p.isolatePid, false);
    QCOMPARE(p.isolateIpc, false);
    QCOMPARE(p.isolateNetwork, false);
    QCOMPARE(p.bindHomeRw, true);
    QVERIFY(p.extraBindPaths.isEmpty());

    // Verify another browser matches the same shape.
    auto p2 = AppLaunchProfile::profileFor(QStringLiteral("chromium"));
    QCOMPARE(p2.isolatePid, false);
    QCOMPARE(p2.isolateIpc, false);
    QCOMPARE(p2.bindHomeRw, true);
}

void TestAppLaunchProfile::testKnownElectronIde() {
    auto p = AppLaunchProfile::profileFor(QStringLiteral("code"));
    QCOMPARE(p.isolatePid, false);
    QCOMPARE(p.isolateIpc, false);
    QCOMPARE(p.isolateNetwork, false);
    QCOMPARE(p.bindHomeRw, true);
    QVERIFY(p.extraBindPaths.isEmpty());
}

void TestAppLaunchProfile::testKnownJetbrainsIde() {
    auto p = AppLaunchProfile::profileFor(QStringLiteral("idea"));
    QCOMPARE(p.isolatePid, false);
    QCOMPARE(p.isolateIpc, true);
    QCOMPARE(p.isolateNetwork, false);
    QCOMPARE(p.bindHomeRw, true);
    QVERIFY(p.extraBindPaths.isEmpty());
}

void TestAppLaunchProfile::testKnownTerminal() {
    // Terminals: strict isolation (pid + ipc), no home rw.
    auto p = AppLaunchProfile::profileFor(QStringLiteral("alacritty"));
    QCOMPARE(p.isolatePid, true);
    QCOMPARE(p.isolateIpc, true);
    QCOMPARE(p.isolateNetwork, false);
    QCOMPARE(p.bindHomeRw, false);
}

void TestAppLaunchProfile::testUnknownApp() {
    // Unknown apps get conservative defaults.
    auto p = AppLaunchProfile::profileFor(QStringLiteral("totally-unknown-app-12345"));
    QCOMPARE(p.isolatePid, true);
    QCOMPARE(p.isolateIpc, true);
    QCOMPARE(p.isolateNetwork, false);
    QCOMPARE(p.isolateFilesystem, false);
    QCOMPARE(p.bindHomeRw, false);
    QVERIFY(p.extraBindPaths.isEmpty());
}

void TestAppLaunchProfile::testHasProfile() {
    QVERIFY(AppLaunchProfile::hasProfile(QStringLiteral("firefox")));
    QVERIFY(AppLaunchProfile::hasProfile(QStringLiteral("code")));
    QVERIFY(AppLaunchProfile::hasProfile(QStringLiteral("idea")));
    QVERIFY(AppLaunchProfile::hasProfile(QStringLiteral("alacritty")));
    QVERIFY(!AppLaunchProfile::hasProfile(QStringLiteral("random-nonexistent")));
    QVERIFY(!AppLaunchProfile::hasProfile(QString()));
}

void TestAppLaunchProfile::testKnownAppIds() {
    auto ids = AppLaunchProfile::knownAppIds();
    QVERIFY(!ids.isEmpty());
    QVERIFY(ids.contains(QStringLiteral("firefox")));
    QVERIFY(ids.contains(QStringLiteral("code")));
    QVERIFY(ids.contains(QStringLiteral("idea")));
    QVERIFY(ids.contains(QStringLiteral("alacritty")));
}

QTEST_MAIN(TestAppLaunchProfile)
#include "tst_applaunchprofile.moc"
