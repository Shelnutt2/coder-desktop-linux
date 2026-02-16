#include <QApplication>
#include <QIcon>
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
#include "webview/AppBrowserWidget.h"
#include "dlp/DlpCompositorWidget.h"
#include "updater/AutoUpdater.h"

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

    // ---- Phase 2 managers ----
    SettingsManager settingsManager;
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

    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreated,
        &app, [](QObject* obj, const QUrl& url) {
            if (!obj) {
                qCritical() << "Failed to load QML:" << url;
                QCoreApplication::exit(-1);
            }
        },
        Qt::QueuedConnection);

    engine.loadFromModule("CoderDesktop", "Main");

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
