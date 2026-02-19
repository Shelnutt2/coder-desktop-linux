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
#include "filesync/FileTransferManager.h"
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

    // =================================================================
    // 8. FileSyncSession — extended
    // =================================================================

    void testStatusStringUnknownNotEmpty() {
        FileSyncSession s;
        s.status = FileSyncStatus::Unknown;
        QVERIFY(!s.statusString().isEmpty());
        QCOMPARE(s.statusCategory(), QStringLiteral("error"));
    }

    void testFileSyncModeValuesDistinct() {
        // Verify enum values exist and are distinct.
        QVERIFY(static_cast<int>(FileSyncMode::TwoWay) !=
                static_cast<int>(FileSyncMode::OneWayLocalToRemote));
        QVERIFY(static_cast<int>(FileSyncMode::OneWayLocalToRemote) !=
                static_cast<int>(FileSyncMode::OneWayRemoteToLocal));
        QVERIFY(static_cast<int>(FileSyncMode::TwoWay) !=
                static_cast<int>(FileSyncMode::OneWayRemoteToLocal));
    }

    void testSessionDefaultValues() {
        // A default-constructed session has sensible defaults.
        FileSyncSession s;
        QVERIFY(s.sessionId.isEmpty());
        QVERIFY(s.localPath.isEmpty());
        QVERIFY(s.workspace.isEmpty());
        QVERIFY(s.remotePath.isEmpty());
        QCOMPARE(s.status, FileSyncStatus::Unknown);
        QCOMPARE(s.mode, FileSyncMode::TwoWay);
        QCOMPARE(s.sizeBytes, 0);
        QCOMPARE(s.conflictCount, 0);
        QVERIFY(!s.paused);
        QVERIFY(s.lastError.isEmpty());
    }

    void testStatusCategoryCoversAllStatuses() {
        // Every status enum must produce a non-empty category string.
        FileSyncSession s;
        const std::initializer_list<FileSyncStatus> allStatuses = {
            FileSyncStatus::Unknown,
            FileSyncStatus::ConnectingAlpha,
            FileSyncStatus::ConnectingBeta,
            FileSyncStatus::Scanning,
            FileSyncStatus::Reconciling,
            FileSyncStatus::StagingAlpha,
            FileSyncStatus::StagingBeta,
            FileSyncStatus::Transitioning,
            FileSyncStatus::Saving,
            FileSyncStatus::Watching,
            FileSyncStatus::Disconnected,
            FileSyncStatus::HaltedOnRootEmptied,
            FileSyncStatus::HaltedOnRootDeletion,
            FileSyncStatus::HaltedOnRootTypeChange,
            FileSyncStatus::WaitingForRescan,
            FileSyncStatus::Paused,
        };
        for (auto st : allStatuses) {
            s.status = st;
            const QString cat = s.statusCategory();
            QVERIFY2(!cat.isEmpty(),
                     qPrintable(
                         QStringLiteral("Empty category for status %1").arg(static_cast<int>(st))));
            // Must be one of the known categories.
            QVERIFY2(cat == QLatin1String("ok") || cat == QLatin1String("working") ||
                         cat == QLatin1String("error") || cat == QLatin1String("paused"),
                     qPrintable(QStringLiteral("Unexpected category '%1'").arg(cat)));
        }
    }

    // =================================================================
    // 9. FileSyncManager — signals and idempotency
    // =================================================================

    void testSetVpnConnectedIdempotent() {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());

        SettingsManager settings(tmpDir.filePath(QStringLiteral("no-policy")),
                                 userSettingsPath(tmpDir));
        MutagenDaemon daemon;
        FileSyncManager manager(&settings, &daemon);

        QSignalSpy spy(&manager, &FileSyncManager::availableChanged);
        QVERIFY(spy.isValid());

        manager.setVpnConnected(true);
        QCOMPARE(spy.count(), 1);

        // Same value again — should NOT emit a second time.
        manager.setVpnConnected(true);
        QCOMPARE(spy.count(), 1);
    }

    void testSetVpnConnectedToggle() {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());

        SettingsManager settings(tmpDir.filePath(QStringLiteral("no-policy")),
                                 userSettingsPath(tmpDir));
        MutagenDaemon daemon;
        FileSyncManager manager(&settings, &daemon);

        QSignalSpy spy(&manager, &FileSyncManager::availableChanged);
        QVERIFY(spy.isValid());

        manager.setVpnConnected(true);
        manager.setVpnConnected(false);
        QCOMPARE(spy.count(), 2);
    }

    void testCountPropertyMatchesSessionCount() {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());

        SettingsManager settings(tmpDir.filePath(QStringLiteral("no-policy")),
                                 userSettingsPath(tmpDir));
        MutagenDaemon daemon;
        FileSyncManager manager(&settings, &daemon);

        // The `count` Q_PROPERTY aliases sessionCount().
        QCOMPARE(manager.property("count").toInt(), manager.sessionCount());
        QCOMPARE(manager.property("count").toInt(), 0);
    }

    void testStatusSummaryEmpty() {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());

        SettingsManager settings(tmpDir.filePath(QStringLiteral("no-policy")),
                                 userSettingsPath(tmpDir));
        MutagenDaemon daemon;
        FileSyncManager manager(&settings, &daemon);

        // Empty model should report "No sync sessions".
        QCOMPARE(manager.statusSummary(), QStringLiteral("No sync sessions"));
    }

    void testPolicyChangedSignalOnSettingsChange() {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());

        SettingsManager settings(tmpDir.filePath(QStringLiteral("no-policy")),
                                 userSettingsPath(tmpDir));
        MutagenDaemon daemon;
        FileSyncManager manager(&settings, &daemon);

        QSignalSpy spy(&manager, &FileSyncManager::policyChanged);
        QVERIFY(spy.isValid());

        // Changing any setting triggers settingsChanged → policyChanged.
        settings.setUserPreference(QStringLiteral("disableFileUpload"), true);
        QVERIFY(spy.count() >= 1);
    }

    void testUploadDownloadBothDisabledAfterConstruction() {
        // Start with no policy, then dynamically disable both directions.
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());

        SettingsManager settings(tmpDir.filePath(QStringLiteral("no-policy")),
                                 userSettingsPath(tmpDir));
        MutagenDaemon daemon;
        FileSyncManager manager(&settings, &daemon);
        manager.setVpnConnected(true);
        QVERIFY(manager.isAvailable());

        // Disable both via user preferences (unlocked).
        settings.setUserPreference(QStringLiteral("disableFileUpload"), true);
        settings.setUserPreference(QStringLiteral("disableFileDownload"), true);
        QVERIFY(!manager.uploadAllowed());
        QVERIFY(!manager.downloadAllowed());
        QVERIFY(!manager.isAvailable());
    }

    void testAvailableTrueRequiresBothVpnAndPolicy() {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());

        // Upload-only policy.
        QJsonObject downloadEntry;
        downloadEntry[QStringLiteral("value")] = true;
        downloadEntry[QStringLiteral("locked")] = true;

        QJsonObject settingsObj;
        settingsObj[QStringLiteral("disableFileDownload")] = downloadEntry;

        const QString mdmPath = writePolicyFile(tmpDir, settingsObj);
        SettingsManager settings(mdmPath, userSettingsPath(tmpDir));
        MutagenDaemon daemon;
        FileSyncManager manager(&settings, &daemon);

        // VPN off → not available.
        manager.setVpnConnected(false);
        QVERIFY(!manager.isAvailable());

        // VPN on, but download disabled (upload still allowed) → available.
        manager.setVpnConnected(true);
        QVERIFY(manager.isAvailable());
    }

    void testNegativeRowIndex() {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());

        SettingsManager settings(tmpDir.filePath(QStringLiteral("no-policy")),
                                 userSettingsPath(tmpDir));
        MutagenDaemon daemon;
        FileSyncManager manager(&settings, &daemon);

        // Negative row should return invalid.
        QModelIndex idx = manager.index(-1, 0);
        QVERIFY(!manager.data(idx, FileSyncManager::SessionIdRole).isValid());
    }

    void testDataWithInvalidRole() {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());

        SettingsManager settings(tmpDir.filePath(QStringLiteral("no-policy")),
                                 userSettingsPath(tmpDir));
        MutagenDaemon daemon;
        FileSyncManager manager(&settings, &daemon);

        QModelIndex idx = manager.index(0, 0);
        // Invalid role number should not crash and should return invalid variant.
        QVERIFY(!manager.data(idx, 9999).isValid());
    }

    void testRowCountWithParentReturnsZero() {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());

        SettingsManager settings(tmpDir.filePath(QStringLiteral("no-policy")),
                                 userSettingsPath(tmpDir));
        MutagenDaemon daemon;
        FileSyncManager manager(&settings, &daemon);

        // A valid parent means we're asking for children of a specific row,
        // which is always 0 for a flat list model.
        QModelIndex parent = manager.index(0, 0);
        QCOMPARE(manager.rowCount(parent), 0);
    }

    // =================================================================
    // 10. FileTransferManager — role names & empty state
    // =================================================================

    void testTransferManagerRoleNames() {
        FileTransferManager manager;
        auto roles = manager.roleNames();

        QVERIFY(roles.values().contains("transferId"));
        QVERIFY(roles.values().contains("localPath"));
        QVERIFY(roles.values().contains("remotePath"));
        QVERIFY(roles.values().contains("isUpload"));
        QVERIFY(roles.values().contains("progress"));
        QVERIFY(roles.values().contains("state"));
        QVERIFY(roles.values().contains("errorMessage"));
        QVERIFY(roles.values().contains("agentHostname"));
    }

    void testTransferManagerEmptyState() {
        FileTransferManager manager;
        QCOMPARE(manager.rowCount(), 0);
        QCOMPARE(manager.activeTransferCount(), 0);
        QCOMPARE(manager.property("count").toInt(), 0);
    }

    void testTransferManagerDownloadCreatesEntry() {
        FileTransferManager manager;

        QSignalSpy countSpy(&manager, &FileTransferManager::transferCountChanged);
        QVERIFY(countSpy.isValid());

        int id = manager.download(QStringLiteral("test.coder"), QStringLiteral("/remote/file.txt"),
                                  QStringLiteral("/tmp/file.txt"));
        QVERIFY(id > 0);
        QCOMPARE(manager.rowCount(), 1);
        QVERIFY(countSpy.count() >= 1);
    }

    void testTransferManagerUploadCreatesEntry() {
        FileTransferManager manager;

        int id = manager.upload(QStringLiteral("test.coder"), QStringLiteral("/tmp/local.txt"),
                                QStringLiteral("/remote/dest.txt"));
        QVERIFY(id > 0);
        QCOMPARE(manager.rowCount(), 1);
    }

    void testTransferManagerCancelNonexistent() {
        FileTransferManager manager;

        // Cancelling a non-existent transfer should not crash.
        manager.cancelTransfer(9999);
        QCOMPARE(manager.rowCount(), 0);
    }

    void testTransferManagerDownloadDataRoles() {
        FileTransferManager manager;

        int id = manager.download(QStringLiteral("test.coder"), QStringLiteral("/remote/file.txt"),
                                  QStringLiteral("/tmp/file.txt"));
        QModelIndex idx = manager.index(0);
        QVERIFY(idx.isValid());

        QCOMPARE(manager.data(idx, FileTransferManager::TransferIdRole).toInt(), id);
        QCOMPARE(manager.data(idx, FileTransferManager::RemotePathRole).toString(),
                 QStringLiteral("/remote/file.txt"));
        QCOMPARE(manager.data(idx, FileTransferManager::LocalPathRole).toString(),
                 QStringLiteral("/tmp/file.txt"));
        QCOMPARE(manager.data(idx, FileTransferManager::IsUploadRole).toBool(), false);
        QCOMPARE(manager.data(idx, FileTransferManager::AgentHostnameRole).toString(),
                 QStringLiteral("test.coder"));
        // Progress should be 0 initially (no bytes transferred).
        QCOMPARE(manager.data(idx, FileTransferManager::ProgressRole).toDouble(), 0.0);
    }

    void testTransferManagerUploadDataRoles() {
        FileTransferManager manager;

        int id = manager.upload(QStringLiteral("ws.coder"), QStringLiteral("/tmp/up.txt"),
                                QStringLiteral("/home/coder/up.txt"));
        QModelIndex idx = manager.index(0);
        QVERIFY(idx.isValid());

        QCOMPARE(manager.data(idx, FileTransferManager::TransferIdRole).toInt(), id);
        QCOMPARE(manager.data(idx, FileTransferManager::IsUploadRole).toBool(), true);
        QCOMPARE(manager.data(idx, FileTransferManager::LocalPathRole).toString(),
                 QStringLiteral("/tmp/up.txt"));
        QCOMPARE(manager.data(idx, FileTransferManager::RemotePathRole).toString(),
                 QStringLiteral("/home/coder/up.txt"));
        QCOMPARE(manager.data(idx, FileTransferManager::AgentHostnameRole).toString(),
                 QStringLiteral("ws.coder"));
    }

    void testTransferManagerInvalidIndex() {
        FileTransferManager manager;

        QModelIndex invalid;
        QVERIFY(!manager.data(invalid, FileTransferManager::TransferIdRole).isValid());
    }

    void testTransferManagerMultipleTransfers() {
        FileTransferManager manager;

        int id1 = manager.download(QStringLiteral("a.coder"), QStringLiteral("/r/1"),
                                   QStringLiteral("/l/1"));
        int id2 = manager.upload(QStringLiteral("b.coder"), QStringLiteral("/l/2"),
                                 QStringLiteral("/r/2"));
        QVERIFY(id1 != id2);
        QCOMPARE(manager.rowCount(), 2);

        // Verify each row returns the correct transfer id.
        QCOMPARE(manager.data(manager.index(0), FileTransferManager::TransferIdRole).toInt(), id1);
        QCOMPARE(manager.data(manager.index(1), FileTransferManager::TransferIdRole).toInt(), id2);
    }

    void testTransferManagerRowCountWithParent() {
        FileTransferManager manager;

        manager.download(QStringLiteral("a.coder"), QStringLiteral("/r/1"), QStringLiteral("/l/1"));

        // A flat model returns 0 for any valid parent.
        QModelIndex parent = manager.index(0);
        QCOMPARE(manager.rowCount(parent), 0);
    }

    void testTransferManagerInvalidRoleReturnsInvalid() {
        FileTransferManager manager;

        manager.download(QStringLiteral("a.coder"), QStringLiteral("/r/1"), QStringLiteral("/l/1"));
        QModelIndex idx = manager.index(0);

        // An unknown role should return an invalid QVariant.
        QVERIFY(!manager.data(idx, 9999).isValid());
    }

    void testTransferManagerCountProperty() {
        FileTransferManager manager;

        QCOMPARE(manager.property("count").toInt(), 0);
        manager.download(QStringLiteral("a.coder"), QStringLiteral("/r/1"), QStringLiteral("/l/1"));
        QCOMPARE(manager.property("count").toInt(), 1);
    }

    // =================================================================
    // 11. FileTransfer struct
    // =================================================================

    void testFileTransferProgressZeroWhenTotalUnknown() {
        FileTransfer t;
        t.bytesTotal = 0;
        t.bytesTransferred = 100;
        QCOMPARE(t.progress(), 0.0);
    }

    void testFileTransferProgressComputation() {
        FileTransfer t;
        t.bytesTotal = 200;
        t.bytesTransferred = 100;
        QCOMPARE(t.progress(), 0.5);
    }

    void testFileTransferProgressComplete() {
        FileTransfer t;
        t.bytesTotal = 1000;
        t.bytesTransferred = 1000;
        QCOMPARE(t.progress(), 1.0);
    }

    void testFileTransferDefaultState() {
        FileTransfer t;
        QCOMPARE(t.id, 0);
        QVERIFY(t.agentHostname.isEmpty());
        QVERIFY(t.localPath.isEmpty());
        QVERIFY(t.remotePath.isEmpty());
        QVERIFY(!t.isUpload);
        QCOMPARE(t.bytesTransferred, 0);
        QCOMPARE(t.bytesTotal, 0);
        QCOMPARE(t.state, FileTransferState::Queued);
        QVERIFY(t.errorMessage.isEmpty());
    }

    void testFileTransferStateEnumValues() {
        // Ensure all state enum values are distinct.
        QSet<int> values;
        values.insert(static_cast<int>(FileTransferState::Queued));
        values.insert(static_cast<int>(FileTransferState::Running));
        values.insert(static_cast<int>(FileTransferState::Completed));
        values.insert(static_cast<int>(FileTransferState::Failed));
        values.insert(static_cast<int>(FileTransferState::Cancelled));
        QCOMPARE(values.size(), 5);
    }

    // =================================================================
    // 12. AgentDirectory DTO — edge cases
    // =================================================================

    void testDirectoryEntryExtraFieldsIgnored() {
        QJsonObject json;
        json[QStringLiteral("name")] = QStringLiteral("test");
        json[QStringLiteral("absolute_path_string")] = QStringLiteral("/test");
        json[QStringLiteral("is_dir")] = true;
        json[QStringLiteral("unexpected_field")] = QStringLiteral("ignored");

        auto entry = DirectoryEntry::fromJson(json);
        QCOMPARE(entry.name, QStringLiteral("test"));
        QCOMPARE(entry.absolutePathString, QStringLiteral("/test"));
        QVERIFY(entry.isDir);
    }

    void testDirectoryEntryWrongTypeForBool() {
        // is_dir as string instead of bool — toBool(false) returns false for non-bool.
        QJsonObject json;
        json[QStringLiteral("name")] = QStringLiteral("test");
        json[QStringLiteral("absolute_path_string")] = QStringLiteral("/test");
        json[QStringLiteral("is_dir")] = QStringLiteral("true");  // string, not bool

        auto entry = DirectoryEntry::fromJson(json);
        QCOMPARE(entry.name, QStringLiteral("test"));
        // QJsonValue::toBool(false) on a string returns the default (false).
        QVERIFY(!entry.isDir);
    }

    void testDirectoryListingLargeContents() {
        // Verify parsing works for a larger number of entries.
        QJsonArray contents;
        for (int i = 0; i < 100; ++i) {
            QJsonObject entry;
            entry[QStringLiteral("name")] = QStringLiteral("file_%1.txt").arg(i);
            entry[QStringLiteral("absolute_path_string")] =
                QStringLiteral("/dir/file_%1.txt").arg(i);
            entry[QStringLiteral("is_dir")] = false;
            contents.append(entry);
        }

        QJsonObject json;
        json[QStringLiteral("absolute_path")] = QJsonArray({QStringLiteral("dir")});
        json[QStringLiteral("absolute_path_string")] = QStringLiteral("/dir");
        json[QStringLiteral("contents")] = contents;

        auto listing = DirectoryListing::fromJson(json);
        QCOMPARE(listing.contents.size(), 100);
        QCOMPARE(listing.contents[0].name, QStringLiteral("file_0.txt"));
        QCOMPARE(listing.contents[99].name, QStringLiteral("file_99.txt"));
    }

    void testDirectoryEntryNameWithSpecialChars() {
        QJsonObject json;
        json[QStringLiteral("name")] = QStringLiteral("file with spaces & (parens).txt");
        json[QStringLiteral("absolute_path_string")] =
            QStringLiteral("/dir/file with spaces & (parens).txt");
        json[QStringLiteral("is_dir")] = false;

        auto entry = DirectoryEntry::fromJson(json);
        QCOMPARE(entry.name, QStringLiteral("file with spaces & (parens).txt"));
    }
};

QTEST_MAIN(TestFileSync)
#include "tst_filesync.moc"
