#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include "api/dto/AgentDirectory.h"
#include "filesync/FileSyncManager.h"
#include "filesync/FileSyncSession.h"
#include "filesync/MutagenDaemon.h"
#include "settings/SettingsManager.h"

/// Unit tests for file-sync components:
///   - FileSyncSession (status string / category mappings)
///   - AgentDirectory DTOs (JSON parsing)
///   - FileSyncManager (policy enforcement, role names, availability)
class TestFileSync : public QObject {
    Q_OBJECT

private:
    /// Write a JSON policy file into @p dir and return the full path.
    static QString writePolicyFile(const QTemporaryDir& dir, const QJsonObject& settingsObj) {
        QJsonObject root;
        root[QStringLiteral("version")] = 1;
        root[QStringLiteral("settings")] = settingsObj;

        const QString path = dir.filePath(QStringLiteral("policy.json"));
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) return {};
        f.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
        f.close();
        return path;
    }

    /// Return a user-settings path inside @p dir (file need not exist yet).
    static QString userSettingsPath(const QTemporaryDir& dir) {
        return dir.filePath(QStringLiteral("settings.ini"));
    }

private slots:

    // =================================================================
    // 1. FileSyncSession — statusString()
    // =================================================================

    void testStatusStringMapping() {
        FileSyncSession s;

        s.status = FileSyncStatus::Unknown;
        QCOMPARE(s.statusString(), QStringLiteral("Unknown"));

        s.status = FileSyncStatus::ConnectingAlpha;
        QCOMPARE(s.statusString(), QString::fromUtf8("Connecting (local)\xe2\x80\xa6"));

        s.status = FileSyncStatus::ConnectingBeta;
        QCOMPARE(s.statusString(), QString::fromUtf8("Connecting (remote)\xe2\x80\xa6"));

        s.status = FileSyncStatus::Scanning;
        QCOMPARE(s.statusString(), QString::fromUtf8("Scanning\xe2\x80\xa6"));

        s.status = FileSyncStatus::Reconciling;
        QCOMPARE(s.statusString(), QString::fromUtf8("Reconciling\xe2\x80\xa6"));

        s.status = FileSyncStatus::StagingAlpha;
        QCOMPARE(s.statusString(), QString::fromUtf8("Staging (local)\xe2\x80\xa6"));

        s.status = FileSyncStatus::StagingBeta;
        QCOMPARE(s.statusString(), QString::fromUtf8("Staging (remote)\xe2\x80\xa6"));

        s.status = FileSyncStatus::Transitioning;
        QCOMPARE(s.statusString(), QString::fromUtf8("Applying changes\xe2\x80\xa6"));

        s.status = FileSyncStatus::Saving;
        QCOMPARE(s.statusString(), QString::fromUtf8("Saving\xe2\x80\xa6"));

        s.status = FileSyncStatus::Watching;
        QCOMPARE(s.statusString(), QStringLiteral("Synced"));

        s.status = FileSyncStatus::Disconnected;
        QCOMPARE(s.statusString(), QStringLiteral("Disconnected"));

        s.status = FileSyncStatus::HaltedOnRootEmptied;
        QCOMPARE(s.statusString(), QStringLiteral("Halted: root emptied"));

        s.status = FileSyncStatus::HaltedOnRootDeletion;
        QCOMPARE(s.statusString(), QStringLiteral("Halted: root deleted"));

        s.status = FileSyncStatus::HaltedOnRootTypeChange;
        QCOMPARE(s.statusString(), QStringLiteral("Halted: root type changed"));

        s.status = FileSyncStatus::WaitingForRescan;
        QCOMPARE(s.statusString(), QString::fromUtf8("Waiting for rescan\xe2\x80\xa6"));

        s.status = FileSyncStatus::Paused;
        QCOMPARE(s.statusString(), QStringLiteral("Paused"));
    }

    // =================================================================
    // 2. FileSyncSession — statusCategory()
    // =================================================================

    void testStatusCategoryOk() {
        FileSyncSession s;
        s.status = FileSyncStatus::Watching;
        QCOMPARE(s.statusCategory(), QStringLiteral("ok"));
    }

    void testStatusCategoryWorking() {
        FileSyncSession s;

        const std::initializer_list<FileSyncStatus> workingStatuses = {
            FileSyncStatus::ConnectingAlpha, FileSyncStatus::ConnectingBeta,
            FileSyncStatus::Scanning,        FileSyncStatus::Reconciling,
            FileSyncStatus::StagingAlpha,    FileSyncStatus::StagingBeta,
            FileSyncStatus::Transitioning,   FileSyncStatus::Saving,
        };
        for (auto st : workingStatuses) {
            s.status = st;
            QCOMPARE(s.statusCategory(), QStringLiteral("working"));
        }
    }

    void testStatusCategoryError() {
        FileSyncSession s;

        const std::initializer_list<FileSyncStatus> errorStatuses = {
            FileSyncStatus::Unknown,
            FileSyncStatus::Disconnected,
            FileSyncStatus::HaltedOnRootEmptied,
            FileSyncStatus::HaltedOnRootDeletion,
            FileSyncStatus::HaltedOnRootTypeChange,
            FileSyncStatus::WaitingForRescan,
        };
        for (auto st : errorStatuses) {
            s.status = st;
            QCOMPARE(s.statusCategory(), QStringLiteral("error"));
        }
    }

    void testStatusCategoryPaused() {
        FileSyncSession s;
        s.status = FileSyncStatus::Paused;
        QCOMPARE(s.statusCategory(), QStringLiteral("paused"));
    }

    // =================================================================
    // 3. AgentDirectory DTOs — DirectoryEntry::fromJson()
    // =================================================================

    void testDirectoryEntryFromJsonDir() {
        QJsonObject json;
        json[QStringLiteral("name")] = QStringLiteral("src");
        json[QStringLiteral("absolute_path_string")] = QStringLiteral("/home/coder/project/src");
        json[QStringLiteral("is_dir")] = true;

        auto entry = DirectoryEntry::fromJson(json);
        QCOMPARE(entry.name, QStringLiteral("src"));
        QCOMPARE(entry.absolutePathString, QStringLiteral("/home/coder/project/src"));
        QVERIFY(entry.isDir);
    }

    void testDirectoryEntryFromJsonFile() {
        QJsonObject json;
        json[QStringLiteral("name")] = QStringLiteral("README.md");
        json[QStringLiteral("absolute_path_string")] =
            QStringLiteral("/home/coder/project/README.md");
        json[QStringLiteral("is_dir")] = false;

        auto entry = DirectoryEntry::fromJson(json);
        QCOMPARE(entry.name, QStringLiteral("README.md"));
        QCOMPARE(entry.absolutePathString, QStringLiteral("/home/coder/project/README.md"));
        QVERIFY(!entry.isDir);
    }

    void testDirectoryEntryMissingFields() {
        // Missing fields should produce safe defaults.
        QJsonObject json;
        auto entry = DirectoryEntry::fromJson(json);
        QVERIFY(entry.name.isEmpty());
        QVERIFY(entry.absolutePathString.isEmpty());
        QVERIFY(!entry.isDir);
    }

    // =================================================================
    // 4. AgentDirectory DTOs — DirectoryListing::fromJson()
    // =================================================================

    void testDirectoryListingFromJson() {
        QJsonArray contents;
        {
            QJsonObject dir;
            dir[QStringLiteral("name")] = QStringLiteral("src");
            dir[QStringLiteral("absolute_path_string")] = QStringLiteral("/home/coder/src");
            dir[QStringLiteral("is_dir")] = true;
            contents.append(dir);
        }
        {
            QJsonObject file;
            file[QStringLiteral("name")] = QStringLiteral("README.md");
            file[QStringLiteral("absolute_path_string")] = QStringLiteral("/home/coder/README.md");
            file[QStringLiteral("is_dir")] = false;
            contents.append(file);
        }

        QJsonObject json;
        json[QStringLiteral("absolute_path")] =
            QJsonArray({QStringLiteral("home"), QStringLiteral("coder")});
        json[QStringLiteral("absolute_path_string")] = QStringLiteral("/home/coder");
        json[QStringLiteral("contents")] = contents;

        auto listing = DirectoryListing::fromJson(json);
        QCOMPARE(listing.absolutePathString, QStringLiteral("/home/coder"));
        QCOMPARE(listing.absolutePath.size(), 2);
        QCOMPARE(listing.absolutePath[0], QStringLiteral("home"));
        QCOMPARE(listing.absolutePath[1], QStringLiteral("coder"));
        QCOMPARE(listing.contents.size(), 2);
        QCOMPARE(listing.contents[0].name, QStringLiteral("src"));
        QVERIFY(listing.contents[0].isDir);
        QCOMPARE(listing.contents[1].name, QStringLiteral("README.md"));
        QVERIFY(!listing.contents[1].isDir);
    }

    void testEmptyDirectoryListing() {
        QJsonObject json;
        json[QStringLiteral("absolute_path")] = QJsonArray();
        json[QStringLiteral("absolute_path_string")] = QStringLiteral("/");
        json[QStringLiteral("contents")] = QJsonArray();

        auto listing = DirectoryListing::fromJson(json);
        QCOMPARE(listing.absolutePathString, QStringLiteral("/"));
        QVERIFY(listing.absolutePath.isEmpty());
        QVERIFY(listing.contents.isEmpty());
    }

    void testDirectoryListingMissingFields() {
        // Completely empty JSON — should not crash.
        QJsonObject json;
        auto listing = DirectoryListing::fromJson(json);
        QVERIFY(listing.absolutePathString.isEmpty());
        QVERIFY(listing.absolutePath.isEmpty());
        QVERIFY(listing.contents.isEmpty());
    }

    // =================================================================
    // 5. FileSyncManager — policy enforcement
    // =================================================================

    void testDefaultPolicyAllowsAll() {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());

        SettingsManager settings(tmpDir.filePath(QStringLiteral("no-policy")),
                                 userSettingsPath(tmpDir));
        MutagenDaemon daemon;
        FileSyncManager manager(&settings, &daemon);

        QVERIFY(manager.uploadAllowed());
        QVERIFY(manager.downloadAllowed());
    }

    void testPolicyUploadDisabled() {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());

        QJsonObject uploadEntry;
        uploadEntry[QStringLiteral("value")] = true;
        uploadEntry[QStringLiteral("locked")] = true;

        QJsonObject settingsObj;
        settingsObj[QStringLiteral("disableFileUpload")] = uploadEntry;

        const QString mdmPath = writePolicyFile(tmpDir, settingsObj);
        SettingsManager settings(mdmPath, userSettingsPath(tmpDir));
        MutagenDaemon daemon;
        FileSyncManager manager(&settings, &daemon);

        QVERIFY(!manager.uploadAllowed());
        QVERIFY(manager.downloadAllowed());
    }

    void testPolicyDownloadDisabled() {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());

        QJsonObject downloadEntry;
        downloadEntry[QStringLiteral("value")] = true;
        downloadEntry[QStringLiteral("locked")] = true;

        QJsonObject settingsObj;
        settingsObj[QStringLiteral("disableFileDownload")] = downloadEntry;

        const QString mdmPath = writePolicyFile(tmpDir, settingsObj);
        SettingsManager settings(mdmPath, userSettingsPath(tmpDir));
        MutagenDaemon daemon;
        FileSyncManager manager(&settings, &daemon);

        QVERIFY(manager.uploadAllowed());
        QVERIFY(!manager.downloadAllowed());
    }

    void testPolicyBothDisabledNotAvailable() {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());

        QJsonObject uploadEntry;
        uploadEntry[QStringLiteral("value")] = true;
        uploadEntry[QStringLiteral("locked")] = true;
        QJsonObject downloadEntry;
        downloadEntry[QStringLiteral("value")] = true;
        downloadEntry[QStringLiteral("locked")] = true;

        QJsonObject settingsObj;
        settingsObj[QStringLiteral("disableFileUpload")] = uploadEntry;
        settingsObj[QStringLiteral("disableFileDownload")] = downloadEntry;

        const QString mdmPath = writePolicyFile(tmpDir, settingsObj);
        SettingsManager settings(mdmPath, userSettingsPath(tmpDir));
        MutagenDaemon daemon;
        FileSyncManager manager(&settings, &daemon);
        manager.setVpnConnected(true);

        QVERIFY(!manager.isAvailable());
    }

    void testVpnRequiredForAvailability() {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());

        SettingsManager settings(tmpDir.filePath(QStringLiteral("no-policy")),
                                 userSettingsPath(tmpDir));
        MutagenDaemon daemon;
        FileSyncManager manager(&settings, &daemon);

        // VPN disconnected → not available.
        manager.setVpnConnected(false);
        QVERIFY(!manager.isAvailable());

        // VPN connected → available (policy allows both directions).
        manager.setVpnConnected(true);
        QVERIFY(manager.isAvailable());
    }

    void testAvailableChangedSignalOnVpn() {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());

        SettingsManager settings(tmpDir.filePath(QStringLiteral("no-policy")),
                                 userSettingsPath(tmpDir));
        MutagenDaemon daemon;
        FileSyncManager manager(&settings, &daemon);

        QSignalSpy spy(&manager, &FileSyncManager::availableChanged);
        QVERIFY(spy.isValid());

        manager.setVpnConnected(true);
        // setVpnConnected should trigger availableChanged.
        QVERIFY(spy.count() >= 1);
    }

    // =================================================================
    // 6. FileSyncManager — role names
    // =================================================================

    void testRoleNames() {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());

        SettingsManager settings(tmpDir.filePath(QStringLiteral("no-policy")),
                                 userSettingsPath(tmpDir));
        MutagenDaemon daemon;
        FileSyncManager manager(&settings, &daemon);

        auto roles = manager.roleNames();
        QVERIFY(roles.values().contains("sessionId"));
        QVERIFY(roles.values().contains("localPath"));
        QVERIFY(roles.values().contains("workspace"));
        QVERIFY(roles.values().contains("remotePath"));
        QVERIFY(roles.values().contains("status"));
        QVERIFY(roles.values().contains("statusString"));
        QVERIFY(roles.values().contains("statusCategory"));
        QVERIFY(roles.values().contains("sizeBytes"));
        QVERIFY(roles.values().contains("conflictCount"));
        QVERIFY(roles.values().contains("paused"));
        QVERIFY(roles.values().contains("mode"));
    }

    // =================================================================
    // 7. FileSyncManager — empty model
    // =================================================================

    void testEmptyModelRowCount() {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());

        SettingsManager settings(tmpDir.filePath(QStringLiteral("no-policy")),
                                 userSettingsPath(tmpDir));
        MutagenDaemon daemon;
        FileSyncManager manager(&settings, &daemon);

        QCOMPARE(manager.rowCount(), 0);
        QCOMPARE(manager.sessionCount(), 0);
    }

    void testEmptyModelDataReturnsInvalid() {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());

        SettingsManager settings(tmpDir.filePath(QStringLiteral("no-policy")),
                                 userSettingsPath(tmpDir));
        MutagenDaemon daemon;
        FileSyncManager manager(&settings, &daemon);

        // Out-of-bounds index should return an invalid QVariant.
        QModelIndex idx = manager.index(0, 0);
        QVERIFY(!manager.data(idx, FileSyncManager::LocalPathRole).isValid());
    }
};

QTEST_MAIN(TestFileSync)
#include "tst_filesync.moc"
