#include "tray/SystemTrayIcon.h"
#include "data/SessionManager.h"
#include "vpn/VpnBridge.h"

#include <QApplication>
#include <QIcon>
#include <QPainter>
#include <QPixmap>

/// Create a simple fallback icon when no theme icon is available.
static QIcon makeFallbackIcon() {
    QPixmap pix(64, 64);
    pix.fill(QColor(0x7C, 0x5C, 0xFC));  // Coder brand purple
    QPainter p(&pix);
    p.setPen(Qt::white);
    p.setFont(QFont(QStringLiteral("sans-serif"), 36, QFont::Bold));
    p.drawText(pix.rect(), Qt::AlignCenter, QStringLiteral("C"));
    p.end();
    return QIcon(pix);
}

SystemTrayIcon::SystemTrayIcon(VpnBridge* vpn, SessionManager* session, QObject* parent)
    : QSystemTrayIcon(parent), m_vpn(vpn), m_session(session) {
    // Use a themed icon; fall back to a simple branded icon.
    QIcon icon = QIcon::fromTheme(QStringLiteral("network-vpn"));
    if (icon.isNull()) icon = QIcon::fromTheme(QStringLiteral("network-wired"));
    if (icon.isNull()) icon = makeFallbackIcon();
    setIcon(icon);
    setToolTip(QStringLiteral("Coder Desktop"));

    buildMenu();
    setContextMenu(&m_menu);

    connect(m_vpn, &VpnBridge::stateChanged, this, &SystemTrayIcon::onVpnStateChanged);

    show();
}

void SystemTrayIcon::buildMenu() {
    m_connectAction = m_menu.addAction(QStringLiteral("Connect VPN"));
    m_disconnectAction = m_menu.addAction(QStringLiteral("Disconnect VPN"));
    m_disconnectAction->setVisible(false);

    m_menu.addSeparator();

    // Workspaces submenu — empty for now; will be populated by WorkspaceModel.
    m_workspacesMenu = m_menu.addMenu(QStringLiteral("Workspaces"));

    m_menu.addSeparator();

    m_openAction = m_menu.addAction(QStringLiteral("Open Coder Desktop"));
    connect(m_openAction, &QAction::triggered, this, &SystemTrayIcon::showWindowRequested);

    m_settingsAction = m_menu.addAction(QStringLiteral("Settings..."));
    connect(m_settingsAction, &QAction::triggered, this, &SystemTrayIcon::showSettingsRequested);

    m_menu.addSeparator();

    m_quitAction = m_menu.addAction(QStringLiteral("Quit"));
    connect(m_quitAction, &QAction::triggered, qApp, &QApplication::quit);

    connect(m_connectAction, &QAction::triggered, this, &SystemTrayIcon::onConnectClicked);
    connect(m_disconnectAction, &QAction::triggered, this, &SystemTrayIcon::onDisconnectClicked);
}

void SystemTrayIcon::onVpnStateChanged() {
    const bool running = m_vpn->isRunning();
    m_connectAction->setVisible(!running);
    m_disconnectAction->setVisible(running);

    setToolTip(QStringLiteral("Coder Desktop — VPN %1").arg(m_vpn->state()));
}

void SystemTrayIcon::onConnectClicked() {
    if (!m_session || !m_session->isAuthenticated()) {
        // No active session — open the main window so the user can log in.
        emit showWindowRequested();
        return;
    }
    m_vpn->start(m_session->currentUrl(), m_session->sessionToken());
}

void SystemTrayIcon::onDisconnectClicked() {
    m_vpn->stop();
}
