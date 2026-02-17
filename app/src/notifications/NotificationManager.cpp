#include "notifications/NotificationManager.h"

#include <QDebug>

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

NotificationManager::NotificationManager(QObject* parent) : QObject(parent) {}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void NotificationManager::notify(const QString& title, const QString& message,
                                 const QString& category) {
    if (!m_enabled) return;

    // Duplicate suppression: build a key from title + message.
    const QString key = title + QLatin1Char('\n') + message;
    const QDateTime now = QDateTime::currentDateTimeUtc();

    cleanupRecent();

    if (m_recentNotifications.contains(key)) {
        const QDateTime& last = m_recentNotifications[key];
        if (last.msecsTo(now) < m_suppressMs) return;  // duplicate within the suppression window
    }
    m_recentNotifications[key] = now;

    m_lastCategory = category;
    sendViaTray(title, message);
}

bool NotificationManager::isEnabled() const {
    return m_enabled;
}

void NotificationManager::setEnabled(bool enabled) {
    if (m_enabled == enabled) return;
    m_enabled = enabled;
    emit enabledChanged();
}

void NotificationManager::setTrayIcon(QSystemTrayIcon* tray) {
    // Disconnect from previous tray icon, if any.
    if (m_trayIcon) disconnect(m_trayIcon, nullptr, this, nullptr);

    m_trayIcon = tray;

    if (m_trayIcon) {
        connect(m_trayIcon, &QSystemTrayIcon::messageClicked, this,
                [this]() { emit notificationClicked(m_lastCategory); });
    }
}

void NotificationManager::setSuppressDuplicateMs(int ms) {
    m_suppressMs = ms;
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

void NotificationManager::sendViaTray(const QString& title, const QString& message) {
    if (!m_trayIcon) {
        qDebug() << "NotificationManager: no tray icon set, dropping notification:" << title;
        return;
    }
    m_trayIcon->showMessage(title, message, QSystemTrayIcon::Information, 5000);
}

void NotificationManager::cleanupRecent() {
    const QDateTime now = QDateTime::currentDateTimeUtc();
    auto it = m_recentNotifications.begin();
    while (it != m_recentNotifications.end()) {
        if (it.value().msecsTo(now) >= m_suppressMs)
            it = m_recentNotifications.erase(it);
        else
            ++it;
    }
}
