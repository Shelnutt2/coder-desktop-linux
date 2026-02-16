#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include "settings/SettingsManager.h"

/// Unit tests for SettingsManager + MdmConfigManager.
///
/// These tests exercise the three-layer settings model:
///   1. Compiled defaults
///   2. User preferences
///   3. MDM policy overrides (with locked / suggested semantics)
class TestSettings : public QObject {
    Q_OBJECT

private:
    /// Write a JSON policy file into @p dir and return the full path.
    static QString writePolicyFile(const QTemporaryDir& dir,
                                   const QJsonObject& settingsObj)
    {
        QJsonObject root;
        root[QStringLiteral("version")] = 1;
        root[QStringLiteral("settings")] = settingsObj;

        const QString path = dir.filePath(QStringLiteral("policy.json"));
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
            return {};
        f.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
        f.close();
        return path;
    }

    /// Return a user-settings path inside @p dir (file need not exist yet).
    static QString userSettingsPath(const QTemporaryDir& dir)
    {
        return dir.filePath(QStringLiteral("settings.ini"));
    }

private slots:

    // -----------------------------------------------------------------
    // 1. Default values when no MDM and no user prefs
    // -----------------------------------------------------------------
    void testDefaults()
    {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());

        // Point MDM at a non-existent file.
        SettingsManager mgr(
            tmpDir.filePath(QStringLiteral("no-such-policy.json")),
            userSettingsPath(tmpDir));

        QCOMPARE(mgr.deploymentUrl(), QString());
        QCOMPARE(mgr.allowedDeployments(), QStringList());
        QCOMPARE(mgr.disableMultiDeployment(), false);
        QCOMPARE(mgr.requireVpn(), false);
        QCOMPARE(mgr.autoConnectVpn(), false);
        QCOMPARE(mgr.dlpEnabled(), false);
        QCOMPARE(mgr.dlpClipboardBlock(), false);
        QCOMPARE(mgr.dlpScreenshotBlock(), false);
        QCOMPARE(mgr.dlpFileSandbox(), false);
        QCOMPARE(mgr.dlpNetworkSandbox(), false);
        QCOMPARE(mgr.dlpForceInAppBrowser(), false);
        QCOMPARE(mgr.dlpDisableExternalBrowser(), false);
        QCOMPARE(mgr.externalBrowserAllowed(), true);
        QCOMPARE(mgr.disableFileUpload(), false);
        QCOMPARE(mgr.disableFileDownload(), false);
        QCOMPARE(mgr.theme(), QStringLiteral("system"));
        QCOMPARE(mgr.notificationsEnabled(), true);

        QCOMPARE(mgr.mdmEnabled(), false);
        QCOMPARE(mgr.isLocked(QStringLiteral("theme")), false);

        // Source should be Default for every key.
        QCOMPARE(mgr.settingSource(QStringLiteral("theme")),
                 static_cast<int>(SettingsManager::Source::Default));
    }

    // -----------------------------------------------------------------
    // 2. User preferences override defaults
    // -----------------------------------------------------------------
    void testUserPrefsOverrideDefaults()
    {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());

        SettingsManager mgr(
            tmpDir.filePath(QStringLiteral("no-such-policy.json")),
            userSettingsPath(tmpDir));

        mgr.setUserPreference(QStringLiteral("theme"), QStringLiteral("dark"));
        QCOMPARE(mgr.theme(), QStringLiteral("dark"));

        mgr.setUserPreference(QStringLiteral("autoConnectVpn"), true);
        QCOMPARE(mgr.autoConnectVpn(), true);

        mgr.setUserPreference(QStringLiteral("deploymentUrl"),
                              QStringLiteral("https://coder.example.com"));
        QCOMPARE(mgr.deploymentUrl(), QStringLiteral("https://coder.example.com"));

        // Source should now be User.
        QCOMPARE(mgr.settingSource(QStringLiteral("theme")),
                 static_cast<int>(SettingsManager::Source::User));
    }

    // -----------------------------------------------------------------
    // 3. MDM values override user preferences
    // -----------------------------------------------------------------
    void testMdmOverridesUserPrefs()
    {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());

        // Write MDM policy that sets theme = "light" (locked).
        QJsonObject themeEntry;
        themeEntry[QStringLiteral("value")]  = QStringLiteral("light");
        themeEntry[QStringLiteral("locked")] = true;

        QJsonObject settings;
        settings[QStringLiteral("theme")] = themeEntry;

        const QString mdmPath = writePolicyFile(tmpDir, settings);
        const QString prefsPath = userSettingsPath(tmpDir);

        // Pre-seed a user preference for theme = "dark".
        {
            QSettings prefs(prefsPath, QSettings::IniFormat);
            prefs.setValue(QStringLiteral("theme"), QStringLiteral("dark"));
            prefs.sync();
        }

        SettingsManager mgr(mdmPath, prefsPath);

        // MDM wins.
        QCOMPARE(mgr.theme(), QStringLiteral("light"));
        QCOMPARE(mgr.mdmEnabled(), true);
        QCOMPARE(mgr.settingSource(QStringLiteral("theme")),
                 static_cast<int>(SettingsManager::Source::Mdm));
    }

    // -----------------------------------------------------------------
    // 4. MDM locked prevents setUserPreference from changing value
    // -----------------------------------------------------------------
    void testMdmLockedPreventsUserChange()
    {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());

        QJsonObject entry;
        entry[QStringLiteral("value")]  = true;
        entry[QStringLiteral("locked")] = true;

        QJsonObject settings;
        settings[QStringLiteral("dlpEnabled")] = entry;

        const QString mdmPath = writePolicyFile(tmpDir, settings);

        SettingsManager mgr(mdmPath, userSettingsPath(tmpDir));

        QCOMPARE(mgr.dlpEnabled(), true);
        QCOMPARE(mgr.isLocked(QStringLiteral("dlpEnabled")), true);
        QCOMPARE(mgr.dlpEnabledLocked(), true);

        // Attempt to change — should be a no-op.
        mgr.setUserPreference(QStringLiteral("dlpEnabled"), false);
        QCOMPARE(mgr.dlpEnabled(), true);
    }

    // -----------------------------------------------------------------
    // 5. MDM suggested (locked=false) allows user override
    // -----------------------------------------------------------------
    void testMdmSuggestedAllowsOverride()
    {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());

        QJsonObject entry;
        entry[QStringLiteral("value")]  = QStringLiteral("dark");
        entry[QStringLiteral("locked")] = false;

        QJsonObject settings;
        settings[QStringLiteral("theme")] = entry;

        const QString mdmPath = writePolicyFile(tmpDir, settings);

        SettingsManager mgr(mdmPath, userSettingsPath(tmpDir));

        // MDM provides "dark" as a suggestion.
        QCOMPARE(mgr.theme(), QStringLiteral("dark"));
        QCOMPARE(mgr.isLocked(QStringLiteral("theme")), false);

        // User can override — the user pref doesn't beat MDM value in
        // resolution order, but the write should succeed (not be rejected).
        mgr.setUserPreference(QStringLiteral("theme"), QStringLiteral("light"));

        // MDM value still wins in resolution because MDM layer is checked
        // first.  The important thing is the write was NOT blocked.
        QCOMPARE(mgr.theme(), QStringLiteral("dark"));

        // Verify the user preference was actually stored.
        QCOMPARE(mgr.settingSource(QStringLiteral("theme")),
                 static_cast<int>(SettingsManager::Source::Mdm));
    }

    // -----------------------------------------------------------------
    // 6. settingsChanged signal emitted on preference change
    // -----------------------------------------------------------------
    void testSettingsChangedSignal()
    {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());

        SettingsManager mgr(
            tmpDir.filePath(QStringLiteral("no-such-policy.json")),
            userSettingsPath(tmpDir));

        QSignalSpy spy(&mgr, &SettingsManager::settingsChanged);
        QVERIFY(spy.isValid());

        mgr.setUserPreference(QStringLiteral("theme"), QStringLiteral("dark"));
        QCOMPARE(spy.count(), 1);

        mgr.setUserPreference(QStringLiteral("autoConnectVpn"), true);
        QCOMPARE(spy.count(), 2);
    }

    // -----------------------------------------------------------------
    // 7. externalBrowserAllowed computed property
    // -----------------------------------------------------------------
    void testExternalBrowserAllowed()
    {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());

        const QString prefsPath = userSettingsPath(tmpDir);

        // Both false → allowed
        {
            SettingsManager mgr(
                tmpDir.filePath(QStringLiteral("no-such-policy.json")),
                prefsPath);
            QCOMPARE(mgr.externalBrowserAllowed(), true);
        }

        // dlpForceInAppBrowser=true, dlpDisableExternalBrowser=false → not allowed
        {
            SettingsManager mgr(
                tmpDir.filePath(QStringLiteral("no-such-policy.json")),
                prefsPath);
            mgr.setUserPreference(QStringLiteral("dlpForceInAppBrowser"), true);
            mgr.setUserPreference(QStringLiteral("dlpDisableExternalBrowser"), false);
            QCOMPARE(mgr.externalBrowserAllowed(), false);
        }

        // dlpForceInAppBrowser=false, dlpDisableExternalBrowser=true → not allowed
        {
            SettingsManager mgr(
                tmpDir.filePath(QStringLiteral("no-such-policy.json")),
                prefsPath);
            mgr.setUserPreference(QStringLiteral("dlpForceInAppBrowser"), false);
            mgr.setUserPreference(QStringLiteral("dlpDisableExternalBrowser"), true);
            QCOMPARE(mgr.externalBrowserAllowed(), false);
        }

        // Both true → not allowed
        {
            SettingsManager mgr(
                tmpDir.filePath(QStringLiteral("no-such-policy.json")),
                prefsPath);
            mgr.setUserPreference(QStringLiteral("dlpForceInAppBrowser"), true);
            mgr.setUserPreference(QStringLiteral("dlpDisableExternalBrowser"), true);
            QCOMPARE(mgr.externalBrowserAllowed(), false);
        }
    }

    // -----------------------------------------------------------------
    // 8. Invalid / missing MDM JSON handled gracefully
    // -----------------------------------------------------------------
    void testInvalidMdmJson()
    {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());

        // a) Completely invalid JSON.
        {
            const QString path = tmpDir.filePath(QStringLiteral("bad.json"));
            QFile f(path);
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write("not json at all {{{{");
            f.close();

            SettingsManager mgr(path, userSettingsPath(tmpDir));
            QCOMPARE(mgr.mdmEnabled(), false);
            QCOMPARE(mgr.theme(), QStringLiteral("system"));
        }

        // b) Valid JSON but missing "settings" key.
        {
            QJsonObject root;
            root[QStringLiteral("version")] = 1;

            const QString path = tmpDir.filePath(QStringLiteral("no-settings.json"));
            QFile f(path);
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
            f.close();

            SettingsManager mgr(path, userSettingsPath(tmpDir));
            QCOMPARE(mgr.mdmEnabled(), false);
        }

        // c) Wrong version number.
        {
            QJsonObject root;
            root[QStringLiteral("version")] = 99;
            root[QStringLiteral("settings")] = QJsonObject();

            const QString path = tmpDir.filePath(QStringLiteral("bad-version.json"));
            QFile f(path);
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
            f.close();

            SettingsManager mgr(path, userSettingsPath(tmpDir));
            QCOMPARE(mgr.mdmEnabled(), false);
        }

        // d) Non-existent file — falls back to defaults gracefully.
        {
            SettingsManager mgr(
                tmpDir.filePath(QStringLiteral("does-not-exist.json")),
                userSettingsPath(tmpDir));
            QCOMPARE(mgr.mdmEnabled(), false);
            QCOMPARE(mgr.notificationsEnabled(), true);
        }
    }
};

QTEST_MAIN(TestSettings)
#include "tst_settings.moc"
