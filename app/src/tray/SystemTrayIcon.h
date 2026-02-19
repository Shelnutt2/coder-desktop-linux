#ifndef SYSTEMTRAYICON_H
#define SYSTEMTRAYICON_H

#include <QAction>
#include <QMenu>
#include <QSystemTrayIcon>

class FileSyncManager;
class SessionManager;
class VpnBridge;

/// System tray icon with VPN connect/disconnect, workspace submenu,
/// settings action, window toggle, and quit.
class SystemTrayIcon : public QSystemTrayIcon {
    Q_OBJECT

public:
    explicit SystemTrayIcon(VpnBridge* vpn, SessionManager* session,
                            FileSyncManager* fileSync = nullptr, QObject* parent = nullptr);

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
    void updateTooltip();

private:
    void buildMenu();

    VpnBridge* m_vpn = nullptr;             // non-owning
    SessionManager* m_session = nullptr;    // non-owning
    FileSyncManager* m_fileSync = nullptr;  // non-owning
    QMenu m_menu;
    QMenu* m_workspacesMenu = nullptr;      // Qt parent-owned (m_menu)
    QAction* m_connectAction = nullptr;     // Qt parent-owned (m_menu)
    QAction* m_disconnectAction = nullptr;  // Qt parent-owned (m_menu)
    QAction* m_settingsAction = nullptr;    // Qt parent-owned (m_menu)
    QAction* m_openAction = nullptr;        // Qt parent-owned (m_menu)
    QAction* m_quitAction = nullptr;        // Qt parent-owned (m_menu)
};

#endif  // SYSTEMTRAYICON_H
