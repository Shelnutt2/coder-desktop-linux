#ifndef SYSTEMTRAYICON_H
#define SYSTEMTRAYICON_H

#include <QAction>
#include <QMenu>
#include <QSystemTrayIcon>

class VpnBridge;

/// System tray icon with VPN connect/disconnect, workspace submenu,
/// settings action, window toggle, and quit.
class SystemTrayIcon : public QSystemTrayIcon {
    Q_OBJECT

public:
    explicit SystemTrayIcon(VpnBridge* vpn, QObject* parent = nullptr);

    /// Expose the underlying QSystemTrayIcon* so that NotificationManager
    /// (and other components) can use showMessage().
    QSystemTrayIcon* trayIcon() { return this; }

signals:
    /// Emitted when the user selects "Open Coder Desktop".
    void showWindowRequested();
    /// Emitted when the user selects "Settings...".
    void showSettingsRequested();

private slots:
    void onVpnStateChanged();
    void onConnectClicked();
    void onDisconnectClicked();

private:
    void buildMenu();

    VpnBridge* m_vpn = nullptr;
    QMenu      m_menu;
    QMenu*     m_workspacesMenu    = nullptr;
    QAction*   m_connectAction     = nullptr;
    QAction*   m_disconnectAction  = nullptr;
    QAction*   m_settingsAction    = nullptr;
    QAction*   m_openAction        = nullptr;
    QAction*   m_quitAction        = nullptr;
};

#endif // SYSTEMTRAYICON_H
