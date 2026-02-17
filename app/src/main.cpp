#include <QApplication>
#include <QCommandLineParser>
#include <QIcon>
#include <QLoggingCategory>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>

#include "vpn/VpnBridge.h"
#include "tray/SystemTrayIcon.h"

#include "settings/SettingsManager.h"
#include "data/SecureStorage.h"
#include "api/CoderApiClient.h"
#include "data/SessionManager.h"
#include "models/WorkspaceModel.h"
#include "models/PeerModel.h"
#include "notifications/NotificationManager.h"
#include "models/TaskModel.h"
#include "api/dto/Workspace.h"
#include "api/dto/Task.h"
#include "webview/AppBrowserWidget.h"
#include "dlp/DlpCompositorWidget.h"
#include "updater/AutoUpdater.h"
#include "auth/LoginFlowController.h"
#include "api/PollingController.h"

#ifdef HAS_WEBENGINE
#include <QtWebEngineQuick/qtwebenginequickglobal.h>
#endif

int main(int argc, char* argv[])
{
    // QtWebEngine requires initialization before QApplication is created.
#ifdef HAS_WEBENGINE
    QtWebEngineQuick::initialize();
#endif

    QApplication app(argc, argv);
    app.setApplicationVersion(QStringLiteral(APP_VERSION));
    app.setApplicationName(QStringLiteral("Coder Desktop"));
    app.setOrganizationName(QStringLiteral("Coder"));
    app.setOrganizationDomain(QStringLiteral("coder.com"));
    app.setWindowIcon(QIcon::fromTheme(QStringLiteral("network-vpn")));

    // Keep the app running when the last window is closed (tray keeps it alive).
    app.setQuitOnLastWindowClosed(false);

    // Default to the Fusion style for a consistent cross-desktop look.
    QQuickStyle::setStyle(QStringLiteral("Fusion"));

    // ---- VPN bridge (D-Bus client) ----
    VpnBridge vpnBridge;

    // ---- CLI argument parsing ----
    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Coder Desktop for Linux"));
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption verboseOption(
        QStringList() << QStringLiteral("v") << QStringLiteral("verbose"),
        QStringLiteral("Enable verbose/debug logging"));
    parser.addOption(verboseOption);
    parser.process(app);

    // ---- Phase 2 managers ----
    SettingsManager settingsManager;

    // ---- Logging configuration ----
    const bool verbose = parser.isSet(verboseOption)
        || settingsManager.verbose();

    if (verbose) {
        QLoggingCategory::setFilterRules(QStringLiteral("*.debug=true"));
        qSetMessagePattern(QStringLiteral(
            "[%{time yyyy-MM-dd hh:mm:ss.zzz}] [%{type}] %{category}: %{message}"));
        qDebug() << "Verbose logging enabled";
    } else {
        qSetMessagePattern(QStringLiteral(
            "[%{time hh:mm:ss}] [%{type}] %{message}"));
    }

    SecureStorage secureStorage;
    CoderApiClient apiClient;
    SessionManager sessionManager(apiClient, secureStorage);
    WorkspaceModel workspaceModel;
    PeerModel peerModel;
    NotificationManager notificationManager;

    // ---- Auto-updater ----
    AutoUpdater autoUpdater(QStringLiteral(APP_VERSION), &settingsManager);
    QObject::connect(&autoUpdater, &AutoUpdater::updateAvailable,
                     [](const QString& version, const QString& url) {
        qInfo() << "Update available:" << version << "—" << url;
    });

    TaskModel taskModel;

    // ---- Polling controller (auto-refresh, caching, notifications) ----
    PollingController pollingController(apiClient, workspaceModel, taskModel,
                                        notificationManager, settingsManager);

    // NOTE: PollingController's constructor already connects
    // apiClient.workspacesReceived/tasksReceived to its handler slots.

    // Show loading state and handle errors for workspace/task fetches.
    QObject::connect(&apiClient, &CoderApiClient::requestFailed,
                     [&workspaceModel, &taskModel](const QString &endpoint,
                                                   int /*statusCode*/,
                                                   const QString &errorMessage) {
        if (endpoint.contains(QLatin1String("workspaces"))) {
            workspaceModel.setLoading(false);
            workspaceModel.setErrorMessage(errorMessage);
        }
        if (endpoint.contains(QLatin1String("tasks"))) {
            taskModel.setLoading(false);
            taskModel.setErrorMessage(errorMessage);
        }
    });

    // Start/stop polling on auth state changes.
    QObject::connect(&sessionManager, &SessionManager::authStateChanged,
                     [&pollingController, &sessionManager]() {
        if (sessionManager.isAuthenticated())
            pollingController.start();
        else
            pollingController.stop();
    });
    QObject::connect(&sessionManager, &SessionManager::tokenExpired,
                     &pollingController, &PollingController::stop);

    // Re-fetch after workspace actions (start/stop/update/delete).
    QObject::connect(&apiClient, &CoderApiClient::workspaceActionCompleted,
                     &pollingController, &PollingController::refreshNow);

    // Wire notificationsEnabled setting to NotificationManager.
    auto syncNotifEnabled = [&]() {
        notificationManager.setEnabled(settingsManager.notificationsEnabled());
    };
    syncNotifEnabled();
    QObject::connect(&settingsManager, &SettingsManager::settingsChanged,
                     syncNotifEnabled);

    // ---- Login flow (browser-based auth) ----
    LoginFlowController loginFlowController(sessionManager);

    // ---- Phase 3 components ----
    AppBrowserWidget appBrowser;

    // ---- DLP compositor ----
    DlpCompositorWidget dlpCompositor;

    // Wire DLP settings changes to the compositor policy.
    QObject::connect(&settingsManager, &SettingsManager::settingsChanged, [&]() {
        if (dlpCompositor.isRunning()) {
            dlpCompositor.updatePolicy(
                settingsManager.dlpClipboardBlock(),
                settingsManager.dlpClipboardBlock(),  // both directions
                true,  // screenshot always blocked
                settingsManager.dlpFileSandbox(),
                settingsManager.dlpNetworkSandbox()
            );
        }
    });

    // ---- QML engine ----
    // All context properties must be set BEFORE loadFromModule() so they
    // are available when QML bindings are first evaluated.
    QQmlApplicationEngine engine;

    engine.rootContext()->setContextProperty(
        QStringLiteral("vpnBridge"), &vpnBridge);
    engine.rootContext()->setContextProperty(
        QStringLiteral("autoUpdater"), &autoUpdater);
    engine.rootContext()->setContextProperty(
        QStringLiteral("settingsManager"), &settingsManager);
    engine.rootContext()->setContextProperty(
        QStringLiteral("sessionManager"), &sessionManager);
    engine.rootContext()->setContextProperty(
        QStringLiteral("apiClient"), &apiClient);
    engine.rootContext()->setContextProperty(
        QStringLiteral("workspaceModel"), &workspaceModel);
    engine.rootContext()->setContextProperty(
        QStringLiteral("peerModel"), &peerModel);
    engine.rootContext()->setContextProperty(
        QStringLiteral("notificationManager"), &notificationManager);
    engine.rootContext()->setContextProperty(
        QStringLiteral("taskModel"), &taskModel);
    engine.rootContext()->setContextProperty(
        QStringLiteral("appBrowser"), &appBrowser);
    engine.rootContext()->setContextProperty(
        QStringLiteral("dlpCompositor"), &dlpCompositor);
    engine.rootContext()->setContextProperty(
        QStringLiteral("loginFlowController"), &loginFlowController);
    engine.rootContext()->setContextProperty(
        QStringLiteral("pollingController"), &pollingController);

    // Load Main.qml directly from compiled-in Qt resources.  Using a
    // resource URL instead of engine.loadFromModule() avoids the need for
    // a qmldir file on disk, so the installed binary works from any prefix.
    const QUrl mainQml(QStringLiteral("qrc:/CoderDesktop/qml/Main.qml"));

    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreated,
        &app, [&mainQml](QObject* obj, const QUrl& url) {
            if (!obj && url == mainQml) {
                qCritical() << "Failed to load QML:" << url;
                QCoreApplication::exit(-1);
            }
        },
        Qt::QueuedConnection);

    engine.load(mainQml);

    // ---- System tray ----
    SystemTrayIcon tray(&vpnBridge);
    notificationManager.setTrayIcon(tray.trayIcon());

    QObject::connect(&tray, &SystemTrayIcon::showWindowRequested,
                     &engine, [&engine]() {
        // Raise the first root window.
        const auto roots = engine.rootObjects();
        for (auto* obj : roots) {
            if (auto* w = qobject_cast<QQuickWindow*>(obj)) {
                w->show();
                w->raise();
                w->requestActivate();
                break;
            }
        }
    });

    // If already authenticated from a saved session, start polling immediately.
    if (sessionManager.isAuthenticated()) {
        pollingController.start();
    }

    return app.exec();
}
