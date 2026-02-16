#include "vpn/DnsManager.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QStandardPaths>
#include <QTextStream>

// ---------------------------------------------------------------------------
// Construction & detection
// ---------------------------------------------------------------------------

DnsManager::DnsManager()
    : m_backend(detectBackend())
{
    qDebug() << "DnsManager: detected backend"
             << static_cast<int>(m_backend);
}

DnsManager::Backend DnsManager::detectBackend()
{
    if (!QStandardPaths::findExecutable(QStringLiteral("resolvconf")).isEmpty())
        return Backend::Resolvconf;
    if (!QStandardPaths::findExecutable(QStringLiteral("resolvectl")).isEmpty())
        return Backend::Resolvectl;
    if (QFile::exists(QStringLiteral("/etc/resolv.conf")))
        return Backend::DirectFile;
    return Backend::None;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

bool DnsManager::configure(const QStringList& nameservers,
                           const QStringList& searchDomains,
                           const QString& iface)
{
    m_interface = iface;

    switch (m_backend) {
    case Backend::Resolvconf:
        return configureResolvconf(nameservers, searchDomains, iface);
    case Backend::Resolvectl:
        return configureResolvectl(nameservers, searchDomains, iface);
    case Backend::DirectFile:
        return configureDirectFile(nameservers, searchDomains);
    case Backend::None:
        qWarning() << "DnsManager: no DNS backend available";
        return false;
    }
    return false;
}

void DnsManager::teardown()
{
    switch (m_backend) {
    case Backend::Resolvconf:  teardownResolvconf();  break;
    case Backend::Resolvectl:  teardownResolvectl();  break;
    case Backend::DirectFile:  teardownDirectFile();   break;
    case Backend::None: break;
    }
}

// ---------------------------------------------------------------------------
// resolvconf backend
// ---------------------------------------------------------------------------

bool DnsManager::configureResolvconf(const QStringList& ns,
                                     const QStringList& sd,
                                     const QString& iface)
{
    // Build the resolv.conf fragment to pipe into resolvconf.
    QString fragment;
    for (const QString& s : ns)
        fragment += QStringLiteral("nameserver %1\n").arg(s.trimmed());
    if (!sd.isEmpty())
        fragment += QStringLiteral("search %1\n").arg(sd.join(QLatin1Char(' ')));

    QProcess proc;
    proc.start(QStringLiteral("resolvconf"),
               {QStringLiteral("-a"),
                QStringLiteral("tun.%1").arg(iface),
                QStringLiteral("-m"), QStringLiteral("0"),
                QStringLiteral("-x")});
    if (!proc.waitForStarted(3000)) {
        qWarning() << "DnsManager: resolvconf failed to start";
        return false;
    }
    proc.write(fragment.toUtf8());
    proc.closeWriteChannel();
    proc.waitForFinished(5000);

    if (proc.exitCode() != 0) {
        qWarning() << "DnsManager: resolvconf exited with"
                    << proc.exitCode() << proc.readAllStandardError();
        return false;
    }
    return true;
}

void DnsManager::teardownResolvconf()
{
    QProcess::execute(QStringLiteral("resolvconf"),
                      {QStringLiteral("-d"),
                       QStringLiteral("tun.%1").arg(m_interface),
                       QStringLiteral("-f")});
}

// ---------------------------------------------------------------------------
// resolvectl backend (systemd-resolved)
// ---------------------------------------------------------------------------

bool DnsManager::configureResolvectl(const QStringList& ns,
                                     const QStringList& sd,
                                     const QString& iface)
{
    // resolvectl dns <iface> <server1> <server2> ...
    QStringList dnsArgs = {QStringLiteral("dns"), iface};
    dnsArgs.append(ns);

    if (QProcess::execute(QStringLiteral("resolvectl"), dnsArgs) != 0) {
        qWarning() << "DnsManager: resolvectl dns failed";
        return false;
    }

    // resolvectl domain <iface> <domain1> <domain2> ...
    if (!sd.isEmpty()) {
        QStringList domArgs = {QStringLiteral("domain"), iface};
        domArgs.append(sd);
        if (QProcess::execute(QStringLiteral("resolvectl"), domArgs) != 0) {
            qWarning() << "DnsManager: resolvectl domain failed";
            return false;
        }
    }

    return true;
}

void DnsManager::teardownResolvectl()
{
    QProcess::execute(QStringLiteral("resolvectl"),
                      {QStringLiteral("revert"), m_interface});
}

// ---------------------------------------------------------------------------
// Direct /etc/resolv.conf backend (last resort)
// ---------------------------------------------------------------------------

bool DnsManager::configureDirectFile(const QStringList& ns,
                                     const QStringList& sd)
{
    const QString resolvPath = QStringLiteral("/etc/resolv.conf");
    m_backupPath = QStringLiteral("/etc/resolv.conf.coder-backup");

    // Back up the original file (only if a backup doesn't already exist).
    if (!QFile::exists(m_backupPath)) {
        if (!QFile::copy(resolvPath, m_backupPath)) {
            qWarning() << "DnsManager: failed to back up" << resolvPath;
            return false;
        }
    }

    // Read current contents.
    QFile origFile(m_backupPath);
    QString original;
    if (origFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        original = QTextStream(&origFile).readAll();
        origFile.close();
    }

    // Build new content: our nameservers first, then original lines.
    QString newContent;
    newContent += QStringLiteral("# BEGIN coder-desktop managed block\n");
    for (const QString& s : ns)
        newContent += QStringLiteral("nameserver %1\n").arg(s.trimmed());
    if (!sd.isEmpty())
        newContent += QStringLiteral("search %1\n").arg(sd.join(QLatin1Char(' ')));
    newContent += QStringLiteral("# END coder-desktop managed block\n");
    newContent += original;

    QFile outFile(resolvPath);
    if (!outFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        qWarning() << "DnsManager: failed to write" << resolvPath;
        return false;
    }
    outFile.write(newContent.toUtf8());
    outFile.close();
    return true;
}

void DnsManager::teardownDirectFile()
{
    if (m_backupPath.isEmpty())
        return;

    const QString resolvPath = QStringLiteral("/etc/resolv.conf");
    if (QFile::exists(m_backupPath)) {
        QFile::remove(resolvPath);
        QFile::rename(m_backupPath, resolvPath);
    }
    m_backupPath.clear();
}
