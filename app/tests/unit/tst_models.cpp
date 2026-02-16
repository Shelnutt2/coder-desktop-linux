#include <QJsonObject>
#include <QSignalSpy>
#include <QSystemTrayIcon>
#include <QTest>

#include "models/PeerModel.h"
#include "models/WorkspaceModel.h"
#include "notifications/NotificationManager.h"

/// Unit tests for WorkspaceModel, PeerModel, and NotificationManager.
class TestModels : public QObject {
    Q_OBJECT

private slots:
    // -----------------------------------------------------------------------
    // WorkspaceModel
    // -----------------------------------------------------------------------

    void testWorkspaceRoleNames()
    {
        WorkspaceModel model;
        auto roles = model.roleNames();
        QVERIFY(roles.contains(WorkspaceModel::IdRole));
        QVERIFY(roles.contains(WorkspaceModel::NameRole));
        QVERIFY(roles.contains(WorkspaceModel::StatusStringRole));
        QVERIFY(roles.contains(WorkspaceModel::LastUsedAtRole));
        QCOMPARE(roles.value(WorkspaceModel::NameRole), QByteArray("name"));
    }

    void testWorkspaceSetWorkspaces()
    {
        WorkspaceModel model;
        QSignalSpy countSpy(&model, &WorkspaceModel::countChanged);

        WorkspaceModel::WorkspaceInfo ws;
        ws.id   = QStringLiteral("ws-1");
        ws.name = QStringLiteral("dev");
        ws.ownerName = QStringLiteral("alice");
        ws.status = 0;  // Running

        model.setWorkspaces({ws});

        QCOMPARE(model.rowCount(), 1);
        QCOMPARE(countSpy.count(), 1);

        // Verify data() for several roles.
        QModelIndex idx = model.index(0);
        QCOMPARE(model.data(idx, WorkspaceModel::IdRole).toString(),
                 QStringLiteral("ws-1"));
        QCOMPARE(model.data(idx, WorkspaceModel::NameRole).toString(),
                 QStringLiteral("dev"));
        QCOMPARE(model.data(idx, WorkspaceModel::StatusRole).toInt(), 0);
        QCOMPARE(model.data(idx, WorkspaceModel::StatusStringRole).toString(),
                 QStringLiteral("Running"));
    }

    void testWorkspaceUpdateExisting()
    {
        WorkspaceModel model;

        WorkspaceModel::WorkspaceInfo ws;
        ws.id   = QStringLiteral("ws-1");
        ws.name = QStringLiteral("dev");
        ws.status = 0;  // Running
        model.setWorkspaces({ws});

        QSignalSpy dataSpy(&model, &WorkspaceModel::dataChanged);

        // Update the existing workspace.
        ws.status = 4;  // Failed
        model.updateWorkspace(ws);

        QCOMPARE(model.rowCount(), 1);  // still one entry
        QCOMPARE(dataSpy.count(), 1);
        QCOMPARE(model.data(model.index(0), WorkspaceModel::StatusRole).toInt(), 4);
    }

    void testWorkspaceUpdateAddsNew()
    {
        WorkspaceModel model;
        model.setWorkspaces({});
        QCOMPARE(model.rowCount(), 0);

        QSignalSpy countSpy(&model, &WorkspaceModel::countChanged);

        WorkspaceModel::WorkspaceInfo ws;
        ws.id   = QStringLiteral("ws-new");
        ws.name = QStringLiteral("new-ws");
        model.updateWorkspace(ws);

        QCOMPARE(model.rowCount(), 1);
        QCOMPARE(countSpy.count(), 1);
    }

    void testWorkspaceClear()
    {
        WorkspaceModel model;
        WorkspaceModel::WorkspaceInfo ws;
        ws.id = QStringLiteral("ws-1");
        model.setWorkspaces({ws});
        QCOMPARE(model.rowCount(), 1);

        QSignalSpy countSpy(&model, &WorkspaceModel::countChanged);
        model.clear();
        QCOMPARE(model.rowCount(), 0);
        QCOMPARE(countSpy.count(), 1);
    }

    void testWorkspaceClearEmpty()
    {
        WorkspaceModel model;
        QSignalSpy countSpy(&model, &WorkspaceModel::countChanged);
        model.clear();  // no-op, should not emit
        QCOMPARE(countSpy.count(), 0);
    }

    void testWorkspaceLoadingState()
    {
        WorkspaceModel model;
        QVERIFY(!model.isLoading());

        QSignalSpy spy(&model, &WorkspaceModel::loadingChanged);
        model.setLoading(true);
        QVERIFY(model.isLoading());
        QCOMPARE(spy.count(), 1);

        // Setting same value should not re-emit.
        model.setLoading(true);
        QCOMPARE(spy.count(), 1);
    }

    void testWorkspaceErrorState()
    {
        WorkspaceModel model;
        QVERIFY(model.errorMessage().isEmpty());

        QSignalSpy spy(&model, &WorkspaceModel::errorChanged);
        model.setErrorMessage(QStringLiteral("timeout"));
        QCOMPARE(model.errorMessage(), QStringLiteral("timeout"));
        QCOMPARE(spy.count(), 1);

        model.setErrorMessage(QStringLiteral("timeout"));
        QCOMPARE(spy.count(), 1);
    }

    void testWorkspaceFromJson()
    {
        QJsonObject latestBuild;
        latestBuild[QLatin1String("status")] = QStringLiteral("running");

        QJsonObject obj;
        obj[QLatin1String("id")]            = QStringLiteral("abc-123");
        obj[QLatin1String("name")]          = QStringLiteral("myws");
        obj[QLatin1String("owner_name")]    = QStringLiteral("bob");
        obj[QLatin1String("template_name")] = QStringLiteral("docker");
        obj[QLatin1String("favorite")]      = true;
        obj[QLatin1String("last_used_at")]  = QStringLiteral("2025-01-15T10:30:00Z");
        obj[QLatin1String("latest_build")]  = latestBuild;

        auto info = WorkspaceModel::WorkspaceInfo::fromJson(obj);
        QCOMPARE(info.id, QStringLiteral("abc-123"));
        QCOMPARE(info.name, QStringLiteral("myws"));
        QCOMPARE(info.ownerName, QStringLiteral("bob"));
        QCOMPARE(info.status, 0);  // Running
        QVERIFY(info.favorite);
        QVERIFY(info.lastUsedAt.isValid());
    }

    void testWorkspaceInvalidIndex()
    {
        WorkspaceModel model;
        // data() on invalid index should return empty QVariant.
        QVERIFY(!model.data(model.index(-1), WorkspaceModel::IdRole).isValid());
        QVERIFY(!model.data(model.index(0), WorkspaceModel::IdRole).isValid());
    }

    // -----------------------------------------------------------------------
    // PeerModel
    // -----------------------------------------------------------------------

    void testPeerRoleNames()
    {
        PeerModel model;
        auto roles = model.roleNames();
        QVERIFY(roles.contains(PeerModel::HostnameRole));
        QVERIFY(roles.contains(PeerModel::ConnectionTypeRole));
        QCOMPARE(roles.value(PeerModel::HostnameRole), QByteArray("hostname"));
    }

    void testPeerSetPeers()
    {
        PeerModel model;
        QSignalSpy countSpy(&model, &PeerModel::countChanged);

        PeerModel::PeerInfo p;
        p.workspaceName = QStringLiteral("dev");
        p.agentName     = QStringLiteral("main");
        p.hostname      = QStringLiteral("dev.coder");
        p.status        = 2;
        p.lastPingMs    = 42;
        p.isP2P         = true;

        model.setPeers({p});

        QCOMPARE(model.rowCount(), 1);
        QCOMPARE(countSpy.count(), 1);

        QModelIndex idx = model.index(0);
        QCOMPARE(model.data(idx, PeerModel::HostnameRole).toString(),
                 QStringLiteral("dev.coder"));
        QCOMPARE(model.data(idx, PeerModel::StatusStringRole).toString(),
                 QStringLiteral("Connected"));
        QCOMPARE(model.data(idx, PeerModel::LastPingMsRole).toLongLong(), 42LL);
        QCOMPARE(model.data(idx, PeerModel::ConnectionTypeRole).toString(),
                 QStringLiteral("P2P"));
    }

    void testPeerUpdateByHostname()
    {
        PeerModel model;

        PeerModel::PeerInfo p;
        p.hostname = QStringLiteral("dev.coder");
        p.status   = 1;
        model.setPeers({p});

        QSignalSpy dataSpy(&model, &PeerModel::dataChanged);

        p.status = 2;
        p.lastPingMs = 10;
        model.updatePeer(p);

        QCOMPARE(model.rowCount(), 1);
        QCOMPARE(dataSpy.count(), 1);
        QCOMPARE(model.data(model.index(0), PeerModel::StatusRole).toInt(), 2);
    }

    void testPeerUpdateAddsNew()
    {
        PeerModel model;
        model.setPeers({});

        QSignalSpy countSpy(&model, &PeerModel::countChanged);

        PeerModel::PeerInfo p;
        p.hostname = QStringLiteral("new.coder");
        model.updatePeer(p);

        QCOMPARE(model.rowCount(), 1);
        QCOMPARE(countSpy.count(), 1);
    }

    void testPeerClear()
    {
        PeerModel model;
        PeerModel::PeerInfo p;
        p.hostname = QStringLiteral("a.coder");
        model.setPeers({p});

        QSignalSpy countSpy(&model, &PeerModel::countChanged);
        model.clear();
        QCOMPARE(model.rowCount(), 0);
        QCOMPARE(countSpy.count(), 1);
    }

    void testPeerRelayed()
    {
        PeerModel model;
        PeerModel::PeerInfo p;
        p.hostname = QStringLiteral("relay.coder");
        p.isP2P    = false;
        model.setPeers({p});

        QCOMPARE(model.data(model.index(0), PeerModel::ConnectionTypeRole).toString(),
                 QStringLiteral("Relayed"));
    }

    // -----------------------------------------------------------------------
    // NotificationManager
    // -----------------------------------------------------------------------

    void testNotificationEnabled()
    {
        NotificationManager mgr;
        QVERIFY(mgr.isEnabled());

        QSignalSpy spy(&mgr, &NotificationManager::enabledChanged);
        mgr.setEnabled(false);
        QVERIFY(!mgr.isEnabled());
        QCOMPARE(spy.count(), 1);

        // Same value — no signal.
        mgr.setEnabled(false);
        QCOMPARE(spy.count(), 1);
    }

    void testNotificationDisabledDrops()
    {
        // When disabled, notify() should be a no-op (not crash).
        NotificationManager mgr;
        mgr.setEnabled(false);
        mgr.notify(QStringLiteral("title"), QStringLiteral("msg"));
        // Nothing to assert beyond "no crash".
    }

    void testNotificationDuplicateSuppression()
    {
        NotificationManager mgr;
        // Use a very short suppression window for testing.
        mgr.setSuppressDuplicateMs(100000);  // 100 seconds — will suppress

        // Without a tray icon the notification just gets logged/dropped,
        // but the dedup logic still runs.  We call notify() twice with the
        // same content; internally only the first should pass the dedup check.
        mgr.notify(QStringLiteral("Build"), QStringLiteral("Done"));
        mgr.notify(QStringLiteral("Build"), QStringLiteral("Done"));

        // Different content should not be suppressed.
        mgr.notify(QStringLiteral("Build"), QStringLiteral("Failed"));
        // No crash, dedup path exercised.
    }

    void testNotificationSetTrayIcon()
    {
        NotificationManager mgr;
        // setTrayIcon(nullptr) must not crash.
        mgr.setTrayIcon(nullptr);
        // Setting a real icon (without showing) should also be safe.
        QSystemTrayIcon tray;
        mgr.setTrayIcon(&tray);
        // Replacing with nullptr disconnects cleanly.
        mgr.setTrayIcon(nullptr);
    }
};

QTEST_MAIN(TestModels)
#include "tst_models.moc"
