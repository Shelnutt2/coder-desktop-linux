#ifndef VPNBRIDGE_H
#define VPNBRIDGE_H

#include <QObject>
#include <QString>
#include <QVariantList>

// Forward-declare the C callback types so this header compiles even when
// libcodervpn.h is not yet generated (unit-test builds, IDE indexing).
// The .cpp includes the real header.
extern "C" {
typedef int32_t (*network_settings_cb)(
    const char*, const char*, const char*, const char*, int32_t);
typedef void (*peer_update_cb)(
    const char*, const char*, const char*, int32_t, int64_t, int32_t);
typedef void (*error_cb)(const char*);
typedef void (*log_cb)(int32_t, const char*);

// Go-exported functions from libcodervpn.so
int32_t CoderVPN_Start(const char* coderURL, const char* apiToken,
                       network_settings_cb, peer_update_cb, error_cb, log_cb);
void    CoderVPN_Stop(void);
int32_t CoderVPN_IsRunning(void);
int32_t CoderVPN_UpdateTunFd(int32_t newFd);
}

/// VpnBridge wraps libcodervpn.so and exposes VPN lifecycle to QML / C++.
///
/// Go callbacks arrive on arbitrary goroutine threads. Each static trampoline
/// posts to the Qt event loop via QMetaObject::invokeMethod(Qt::QueuedConnection)
/// so all signal emissions happen on the main thread.
/// VpnBridge is exposed to QML as a context property ("vpnBridge") in main.cpp.
/// We do NOT use QML_ELEMENT/QML_SINGLETON because the singleton lifetime is
/// managed manually (stack-allocated in main()).
class VpnBridge : public QObject {
    Q_OBJECT

public:
    enum State {
        Disconnected,
        Connecting,
        Connected,
        Disconnecting,
    };
    Q_ENUM(State)

    Q_PROPERTY(State state READ state NOTIFY stateChanged)
    Q_PROPERTY(QString stateString READ stateString NOTIFY stateChanged)
    Q_PROPERTY(bool connected READ connected NOTIFY stateChanged)
    Q_PROPERTY(QVariantList peers READ peers NOTIFY peersChanged)

    explicit VpnBridge(QObject* parent = nullptr);
    ~VpnBridge() override;

    /// Singleton accessor (set once in main.cpp).
    static VpnBridge* instance();
    static void setInstance(VpnBridge* inst);

    State state() const { return m_state; }
    QString stateString() const;
    bool connected() const { return m_state == State::Connected; }
    QVariantList peers() const { return m_peers; }

    Q_INVOKABLE void start(const QString& url, const QString& token);
    Q_INVOKABLE void stop();
    Q_INVOKABLE bool isRunning() const;

signals:
    void stateChanged();
    void peersChanged();
    void errorOccurred(const QString& message);
    void logMessage(int level, const QString& message);
    void networkSettingsReceived(const QStringList& addresses,
                                 const QStringList& dnsServers,
                                 const QStringList& searchDomains,
                                 const QStringList& routes,
                                 int mtu);

private:
    // ---- C callback trampolines (static, called from Go goroutines) ----
    static int32_t onNetworkSettings(const char* addresses,
                                     const char* dnsServers,
                                     const char* searchDomains,
                                     const char* routes,
                                     int32_t mtu);
    static void onPeerUpdate(const char* workspaceName,
                             const char* agentName,
                             const char* hostname,
                             int32_t status,
                             int64_t lastPingMs,
                             int32_t isP2P);
    static void onError(const char* message);
    static void onLog(int32_t level, const char* message);

    // ---- Slots driven by trampolines (main-thread) ----
    Q_INVOKABLE void handleNetworkSettings(const QStringList& addresses,
                                           const QStringList& dnsServers,
                                           const QStringList& searchDomains,
                                           const QStringList& routes,
                                           int mtu);
    Q_INVOKABLE void handlePeerUpdate(const QString& workspaceName,
                                      const QString& agentName,
                                      const QString& hostname,
                                      int status, qint64 lastPingMs, bool isP2P);
    Q_INVOKABLE void handleError(const QString& message);
    Q_INVOKABLE void handleLog(int level, const QString& message);

    State m_state = State::Disconnected;
    QVariantList m_peers;

    static VpnBridge* s_instance;
};

#endif // VPNBRIDGE_H
