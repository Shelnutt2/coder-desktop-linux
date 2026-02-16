#ifndef NOTIFICATIONMANAGER_H
#define NOTIFICATIONMANAGER_H

#include <QDateTime>
#include <QHash>
#include <QObject>
#include <QString>
#include <QSystemTrayIcon>

/// Desktop notification manager.
///
/// Uses QSystemTrayIcon::showMessage() as the primary delivery mechanism
/// (since the application already owns a system-tray icon).  Provides
/// duplicate-suppression so the same title+message is not shown more than
/// once within a configurable time window.
class NotificationManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool enabled READ isEnabled WRITE setEnabled NOTIFY enabledChanged)

public:
    /// Notification categories for filtering / click routing.
    enum class Category {
        WorkspaceState,
        BuildComplete,
        VpnState,
        TaskUpdate,
        General,
    };
    Q_ENUM(Category)

    explicit NotificationManager(QObject* parent = nullptr);

    /// Show a desktop notification.
    /// @p category is an optional free-form string used for click routing
    /// and duplicate-suppression grouping.
    Q_INVOKABLE void notify(const QString& title, const QString& message,
                            const QString& category = QString());

    bool isEnabled() const;
    void setEnabled(bool enabled);

    /// Set the system tray icon used for delivering notifications.
    void setTrayIcon(QSystemTrayIcon* tray);

    /// Set the duplicate-suppression window in milliseconds.
    /// Notifications with the same title+message arriving within this window
    /// are silently dropped.  Default: 5 000 ms.
    void setSuppressDuplicateMs(int ms);

signals:
    void enabledChanged();
    /// Emitted when the user clicks a notification balloon.
    void notificationClicked(const QString& category);

private:
    void sendViaTray(const QString& title, const QString& message);
    void cleanupRecent();

    bool m_enabled = true;
    QSystemTrayIcon* m_trayIcon = nullptr;
    int m_suppressMs = 5000;
    /// Maps "title\nmessage" → timestamp of last delivery.
    QHash<QString, QDateTime> m_recentNotifications;
    /// Stores the category of the most-recently sent notification so we can
    /// relay it when the user clicks the balloon.
    QString m_lastCategory;
};

#endif // NOTIFICATIONMANAGER_H
