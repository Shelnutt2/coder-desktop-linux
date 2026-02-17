#include "vpn/VpnBridge.h"

#include <QDBusError>
#include <QDBusPendingReply>
#include <QDebug>

// ---------------------------------------------------------------------------
// D-Bus service coordinates
// ---------------------------------------------------------------------------
static constexpr auto kHelperService = "com.coder.Desktop.Helper";
static constexpr auto kHelperPath = "/com/coder/Desktop/Helper";

// ---------------------------------------------------------------------------
// Construction — connect to the system bus and wire up D-Bus signals
// ---------------------------------------------------------------------------

VpnBridge::VpnBridge(QObject* parent)
    : QObject(parent),
      m_helper(QLatin1String(kHelperService), QLatin1String(kHelperPath),
               QDBusConnection::systemBus()) {
    // Wire D-Bus signals → local slots.
    connect(&m_helper, &ComCoderDesktopHelper1Interface::StateChanged, this,
            &VpnBridge::onStateChanged);
    connect(&m_helper, &ComCoderDesktopHelper1Interface::PeerUpdated, this,
            &VpnBridge::onPeerUpdated);
    connect(&m_helper, &ComCoderDesktopHelper1Interface::LogMessage, this,
            &VpnBridge::onLogMessage);

    // Note: m_helper.isValid() will return false here if the helper service
    // isn't running yet — that's expected.  D-Bus activation will launch the
    // helper on the first method call (Start/Stop/GetStatus).

    // Query current helper state so the UI reflects a pre-existing tunnel
    // (e.g. the user closed the app without stopping the VPN).
    auto* watcher = new QDBusPendingCallWatcher(m_helper.GetStatus(), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, &VpnBridge::onGetStatusFinished);
}

// ---------------------------------------------------------------------------
// Public API — asynchronous D-Bus calls
// ---------------------------------------------------------------------------

void VpnBridge::start(const QString& url, const QString& token) {
    if (m_state != QStringLiteral("disconnected")) {
        qWarning() << "VpnBridge::start called while state is" << m_state;
        return;
    }

    m_state = QStringLiteral("connecting");
    emit stateChanged();

    auto* watcher = new QDBusPendingCallWatcher(m_helper.Start(url, token), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, &VpnBridge::onStartFinished);
}

void VpnBridge::stop() {
    if (m_state == QStringLiteral("disconnected") || m_state == QStringLiteral("disconnecting"))
        return;

    m_state = QStringLiteral("disconnecting");
    emit stateChanged();

    auto* watcher = new QDBusPendingCallWatcher(m_helper.Stop(), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, &VpnBridge::onStopFinished);
}

// ---------------------------------------------------------------------------
// D-Bus signal handlers
// ---------------------------------------------------------------------------

void VpnBridge::onStateChanged(const QString& newState, const QString& errorMessage) {
    if (m_state == newState) return;

    m_state = newState;
    emit stateChanged();

    if (!errorMessage.isEmpty()) emit errorOccurred(errorMessage);
}

void VpnBridge::onPeerUpdated(const QString& workspace, const QString& agent,
                              const QString& hostname, int status, int lastPingMs, bool isP2P) {
    emit peerUpdated(workspace, agent, hostname, status, lastPingMs, isP2P);
}

void VpnBridge::onLogMessage(int level, const QString& message) {
    switch (level) {
        case 0:
            qDebug() << "[vpn]" << message;
            break;
        case 1:
            qInfo() << "[vpn]" << message;
            break;
        case 2:
            qWarning() << "[vpn]" << message;
            break;
        default:
            qCritical() << "[vpn]" << message;
            break;
    }
}

// ---------------------------------------------------------------------------
// Async method-call completion handlers
// ---------------------------------------------------------------------------

void VpnBridge::onStartFinished(QDBusPendingCallWatcher* watcher) {
    QDBusPendingReply<> reply = *watcher;
    watcher->deleteLater();

    if (reply.isError()) {
        qWarning() << "VpnBridge::Start D-Bus error:"
                   << "name=" << reply.error().name() << "message=" << reply.error().message()
                   << "type=" << reply.error().type();
        m_state = QStringLiteral("disconnected");
        emit stateChanged();
        emit errorOccurred(reply.error().message());
    } else {
        qDebug() << "VpnBridge::Start D-Bus call returned successfully";
    }
    // On success the helper will emit StateChanged → onStateChanged().
}

void VpnBridge::onStopFinished(QDBusPendingCallWatcher* watcher) {
    QDBusPendingReply<> reply = *watcher;
    watcher->deleteLater();

    if (reply.isError()) {
        qWarning() << "VpnBridge::Stop D-Bus error:" << reply.error().message();
        emit errorOccurred(reply.error().message());
    }
    // On success the helper will emit StateChanged → onStateChanged().
}

void VpnBridge::onGetStatusFinished(QDBusPendingCallWatcher* watcher) {
    QDBusPendingReply<QString, QString> reply = *watcher;
    watcher->deleteLater();

    if (reply.isError()) {
        // Helper not running or not reachable — stay "disconnected".
        qDebug() << "VpnBridge::GetStatus D-Bus error (helper may not be running):"
                 << reply.error().message();
        return;
    }

    const QString state = reply.argumentAt<0>();
    if (m_state != state) {
        m_state = state;
        emit stateChanged();
    }
}
