#ifndef SYSTEMTRAYICON_H
#define SYSTEMTRAYICON_H

#include <QAction>
#include <QMenu>
#include <QSystemTrayIcon>

class VpnBridge;

/// System tray icon with VPN connect/disconnect, window toggle, and quit.
class SystemTrayIcon : public QSystemTrayIcon {
    Q_OBJECT

public:
    explicit SystemTrayIcon(VpnBridge* vpn, QObject* parent = nullptr);

signals:
    /// Emitted when the user selects "Open Coder Desktop".
    void showWindowRequested();

private slots:
    void onVpnStateChanged();
    void onConnectClicked();
    void onDisconnectClicked();

private:
    void buildMenu();

    VpnBridge* m_vpn = nullptr;
    QMenu      m_menu;
    QAction*   m_connectAction     = nullptr;
    QAction*   m_disconnectAction  = nullptr;
    QAction*   m_openAction        = nullptr;
    QAction*   m_quitAction        = nullptr;
};

#endif // SYSTEMTRAYICON_H
