#include "vpn/VpnBridge.h"
#include "vpn/TunManager.h"
#include "vpn/DnsManager.h"
#include "util/UniqueFd.h"

#include <QDebug>
#include <QMetaObject>
#include <QVariantMap>

// ---------------------------------------------------------------------------
// Singleton bookkeeping
// ---------------------------------------------------------------------------

VpnBridge* VpnBridge::s_instance = nullptr;

VpnBridge* VpnBridge::instance() { return s_instance; }

void VpnBridge::setInstance(VpnBridge* inst) { s_instance = inst; }

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

VpnBridge::VpnBridge(QObject* parent)
    : QObject(parent)
{
    if (!s_instance)
        s_instance = this;
}

VpnBridge::~VpnBridge()
{
    if (s_instance == this)
        s_instance = nullptr;
}

QString VpnBridge::stateString() const
{
    switch (m_state) {
    case State::Disconnected:  return QStringLiteral("Disconnected");
    case State::Connecting:    return QStringLiteral("Connecting");
    case State::Connected:     return QStringLiteral("Connected");
    case State::Disconnecting: return QStringLiteral("Disconnecting");
    }
    return QStringLiteral("Unknown");
}

void VpnBridge::start(const QString& url, const QString& token)
{
    if (m_state != State::Disconnected) {
        qWarning() << "VpnBridge::start called while state is" << stateString();
        return;
    }

    m_state = State::Connecting;
    emit stateChanged();

    const QByteArray urlUtf8   = url.toUtf8();
    const QByteArray tokenUtf8 = token.toUtf8();

    int32_t rc = CoderVPN_Start(
        urlUtf8.constData(),
        tokenUtf8.constData(),
        &VpnBridge::onNetworkSettings,
        &VpnBridge::onPeerUpdate,
        &VpnBridge::onError,
        &VpnBridge::onLog);

    if (rc != 0) {
        m_state = State::Disconnected;
        emit stateChanged();
        emit errorOccurred(QStringLiteral("CoderVPN_Start returned %1").arg(rc));
    }
}

void VpnBridge::stop()
{
    if (m_state == State::Disconnected || m_state == State::Disconnecting)
        return;

    m_state = State::Disconnecting;
    emit stateChanged();

    CoderVPN_Stop();

    m_state = State::Disconnected;
    m_peers.clear();
    emit stateChanged();
    emit peersChanged();
}

bool VpnBridge::isRunning() const
{
    return CoderVPN_IsRunning() != 0;
}

// ---------------------------------------------------------------------------
// Static C callback trampolines — called from Go goroutines
// ---------------------------------------------------------------------------

int32_t VpnBridge::onNetworkSettings(const char* addresses,
                                     const char* dnsServers,
                                     const char* searchDomains,
                                     const char* routes,
                                     int32_t mtu)
{
    auto* self = VpnBridge::s_instance;
    if (!self) return -1;

    // Copy strings to Qt types before crossing thread boundary.
    QStringList addrList   = QString::fromUtf8(addresses).split(QLatin1Char(','), Qt::SkipEmptyParts);
    QStringList dnsList    = QString::fromUtf8(dnsServers).split(QLatin1Char(','), Qt::SkipEmptyParts);
    QStringList searchList = QString::fromUtf8(searchDomains).split(QLatin1Char(','), Qt::SkipEmptyParts);
    QStringList routeList  = QString::fromUtf8(routes).split(QLatin1Char(','), Qt::SkipEmptyParts);

    // Post to main thread.  The callback must return a TUN fd, but we cannot
    // block here waiting for the Qt event loop.  Create the TUN synchronously
    // on *this* goroutine thread and return it; the main thread will receive
    // the configuration asynchronously for DNS / route setup.
    TunManager tunMgr;
    UniqueFd fd = tunMgr.createTun(QStringLiteral("coder0"));

    // Fire-and-forget to main thread for DNS + bookkeeping.
    QMetaObject::invokeMethod(self, "handleNetworkSettings",
                              Qt::QueuedConnection,
                              Q_ARG(QStringList, addrList),
                              Q_ARG(QStringList, dnsList),
                              Q_ARG(QStringList, searchList),
                              Q_ARG(QStringList, routeList),
                              Q_ARG(int, static_cast<int>(mtu)));

    // Ownership of the fd transfers to the Go runtime via CoderVPN_UpdateTunFd.
    return static_cast<int32_t>(fd.release());
}

void VpnBridge::onPeerUpdate(const char* workspaceName,
                             const char* agentName,
                             const char* hostname,
                             int32_t status,
                             int64_t lastPingMs,
                             int32_t isP2P)
{
    auto* self = VpnBridge::s_instance;
    if (!self) return;

    QMetaObject::invokeMethod(self, "handlePeerUpdate",
                              Qt::QueuedConnection,
                              Q_ARG(QString, QString::fromUtf8(workspaceName)),
                              Q_ARG(QString, QString::fromUtf8(agentName)),
                              Q_ARG(QString, QString::fromUtf8(hostname)),
                              Q_ARG(int, static_cast<int>(status)),
                              Q_ARG(qint64, static_cast<qint64>(lastPingMs)),
                              Q_ARG(bool, isP2P != 0));
}

void VpnBridge::onError(const char* message)
{
    auto* self = VpnBridge::s_instance;
    if (!self) return;

    QMetaObject::invokeMethod(self, "handleError",
                              Qt::QueuedConnection,
                              Q_ARG(QString, QString::fromUtf8(message)));
}

void VpnBridge::onLog(int32_t level, const char* message)
{
    auto* self = VpnBridge::s_instance;
    if (!self) return;

    QMetaObject::invokeMethod(self, "handleLog",
                              Qt::QueuedConnection,
                              Q_ARG(int, static_cast<int>(level)),
                              Q_ARG(QString, QString::fromUtf8(message)));
}

// ---------------------------------------------------------------------------
// Main-thread handlers (invoked via QueuedConnection)
// ---------------------------------------------------------------------------

void VpnBridge::handleNetworkSettings(const QStringList& addresses,
                                      const QStringList& dnsServers,
                                      const QStringList& searchDomains,
                                      const QStringList& routes,
                                      int mtu)
{
    Q_UNUSED(routes)
    Q_UNUSED(mtu)

    // Configure DNS via the system resolver cascade.
    DnsManager dns;
    if (!dns.configure(dnsServers, searchDomains, QStringLiteral("coder0"))) {
        qWarning() << "DnsManager::configure failed";
    }

    if (m_state == State::Connecting) {
        m_state = State::Connected;
        emit stateChanged();
    }

    emit networkSettingsReceived(addresses, dnsServers, searchDomains, routes, mtu);
}

void VpnBridge::handlePeerUpdate(const QString& workspaceName,
                                 const QString& agentName,
                                 const QString& hostname,
                                 int status, qint64 lastPingMs, bool isP2P)
{
    QVariantMap peer;
    peer[QStringLiteral("workspaceName")] = workspaceName;
    peer[QStringLiteral("agentName")]     = agentName;
    peer[QStringLiteral("hostname")]      = hostname;
    peer[QStringLiteral("status")]        = status;
    peer[QStringLiteral("lastPingMs")]    = lastPingMs;
    peer[QStringLiteral("isP2P")]         = isP2P;

    // Upsert by hostname.
    for (int i = 0; i < m_peers.size(); ++i) {
        if (m_peers[i].toMap()[QStringLiteral("hostname")] == hostname) {
            m_peers[i] = peer;
            emit peersChanged();
            return;
        }
    }
    m_peers.append(peer);
    emit peersChanged();
}

void VpnBridge::handleError(const QString& message)
{
    qWarning() << "VPN error:" << message;

    // A fatal error moves us back to Disconnected.
    if (m_state != State::Disconnected) {
        m_state = State::Disconnected;
        emit stateChanged();
    }
    emit errorOccurred(message);
}

void VpnBridge::handleLog(int level, const QString& message)
{
    // Forward to Qt logging (levels: 0=DEBUG, 1=INFO, 2=WARN, 3=ERROR).
    switch (level) {
    case 0:  qDebug()    << "[vpn]" << message; break;
    case 1:  qInfo()     << "[vpn]" << message; break;
    case 2:  qWarning()  << "[vpn]" << message; break;
    default: qCritical() << "[vpn]" << message; break;
    }
    emit logMessage(level, message);
}
