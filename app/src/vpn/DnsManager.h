#ifndef DNSMANAGER_H
#define DNSMANAGER_H

#include <QString>
#include <QStringList>

/// DNS configuration manager with a three-backend cascade:
///   1. resolvconf  (if available in PATH)
///   2. resolvectl  (systemd-resolved)
///   3. Direct /etc/resolv.conf rewrite (fallback)
///
/// Detection runs once at construction time.  configure() pushes nameservers
/// and search domains for the VPN interface; teardown() reverts them.
class DnsManager {
public:
    enum class Backend { Resolvconf, Resolvectl, DirectFile, None };

    explicit DnsManager();

    [[nodiscard]] Backend detectedBackend() const { return m_backend; }

    /// Push DNS configuration for the given interface.
    /// @return true on success.
    [[nodiscard]] bool configure(const QStringList& nameservers,
                   const QStringList& searchDomains,
                   const QString& iface = QStringLiteral("coder0"));

    /// Revert DNS configuration set by configure().
    void teardown();

private:
    Backend m_backend = Backend::None;
    QString m_interface;
    QString m_backupPath;  ///< Only used by DirectFile backend.

    static Backend detectBackend();

    bool configureResolvconf(const QStringList& ns,
                             const QStringList& sd,
                             const QString& iface);
    bool configureResolvectl(const QStringList& ns,
                             const QStringList& sd,
                             const QString& iface);
    bool configureDirectFile(const QStringList& ns,
                             const QStringList& sd);

    void teardownResolvconf();
    void teardownResolvectl();
    void teardownDirectFile();
};

#endif // DNSMANAGER_H
