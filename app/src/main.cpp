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

int main(int argc, char* argv[])
{
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

    // ---- VPN bridge singleton ----
    VpnBridge vpnBridge;
    VpnBridge::setInstance(&vpnBridge);

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

    // ---- Wire API fetch signals to models ----
    QObject::connect(&apiClient, &CoderApiClient::workspacesReceived,
                     [&workspaceModel](const QJsonArray &arr) {
        QList<WorkspaceModel::WorkspaceInfo> list;
        list.reserve(arr.size());
        for (const QJsonValue &v : arr)
            list.append(WorkspaceModel::WorkspaceInfo::fromJson(v.toObject()));
        workspaceModel.setWorkspaces(list);
        workspaceModel.setLoading(false);
        workspaceModel.setErrorMessage(QString());
    });

    QObject::connect(&apiClient, &CoderApiClient::tasksReceived,
                     [&taskModel](const QJsonArray &arr) {
        QList<TaskModel::TaskInfo> list;
        list.reserve(arr.size());
        for (const QJsonValue &v : arr)
            list.append(TaskModel::TaskInfo::fromJson(v.toObject()));
        taskModel.setTasks(list);
        taskModel.setLoading(false);
        taskModel.setErrorMessage(QString());
    });

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

    return app.exec();
}
