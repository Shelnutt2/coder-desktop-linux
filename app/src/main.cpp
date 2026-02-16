#include <QApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>

#include "vpn/VpnBridge.h"
#include "tray/SystemTrayIcon.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
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

    // ---- QML engine ----
    QQmlApplicationEngine engine;

    // Expose VpnBridge to QML as a context property (the QML_SINGLETON
    // registration also makes it available via the CoderDesktop import,
    // but a context property is simpler for the Phase 1 skeleton).
    engine.rootContext()->setContextProperty(
        QStringLiteral("vpnBridge"), &vpnBridge);

    const QUrl mainQml(QStringLiteral("qrc:/CoderDesktop/qml/main.qml"));
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreated,
        &app, [&mainQml](QObject* obj, const QUrl& url) {
            if (!obj && url == mainQml)
                QCoreApplication::exit(-1);
        },
        Qt::QueuedConnection);

    engine.loadFromModule("CoderDesktop", "Main");

    // ---- System tray ----
    SystemTrayIcon tray(&vpnBridge);
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
