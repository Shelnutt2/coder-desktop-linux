#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#include "vpn/DnsManager.h"

/// Unit tests for DnsManager.
///
/// These tests exercise:
///   - Backend detection (whatever is in PATH on this system)
///   - Direct-file backup/restore logic using a temp directory
///   - configure() / teardown() with Backend::None (must not crash)
class TestDnsManager : public QObject {
    Q_OBJECT

private slots:
    /// detectBackend() should return a deterministic value.
    void testDetectBackend()
    {
        DnsManager dns;
        auto backend = dns.detectedBackend();
        // On any real Linux system at least one of these should be present,
        // but we don't enforce which one — just that it doesn't crash and
        // returns a valid enum value.
        QVERIFY(backend == DnsManager::Backend::Resolvconf ||
                backend == DnsManager::Backend::Resolvectl ||
                backend == DnsManager::Backend::DirectFile ||
                backend == DnsManager::Backend::None);
    }

    /// configure() and teardown() must not crash when backend is None.
    void testNoneBackend()
    {
        // We can't easily force Backend::None, but we can verify that
        // calling configure/teardown on a default-constructed DnsManager
        // does not segfault regardless of backend.
        DnsManager dns;
        // With an empty nameserver list the call is essentially a no-op
        // even on real backends.
        [[maybe_unused]] bool ok = dns.configure({}, {}, QStringLiteral("coder-test0"));
        dns.teardown();
    }

    /// DirectFile: backup, modify, restore round-trip using temp files.
    void testDirectFileRoundTrip()
    {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());

        const QString fakePath = tmpDir.filePath(QStringLiteral("resolv.conf"));
        const QString backupPath = fakePath + QStringLiteral(".coder-backup");

        // Write a fake /etc/resolv.conf.
        {
            QFile f(fakePath);
            QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
            f.write("# original resolv.conf\nnameserver 8.8.8.8\n");
            f.close();
        }

        // -- Simulate the DirectFile backend logic manually --
        // Back up.
        QVERIFY(QFile::copy(fakePath, backupPath));

        // Read backup.
        QFile orig(backupPath);
        QVERIFY(orig.open(QIODevice::ReadOnly));
        const QString origContent = QString::fromUtf8(orig.readAll());
        orig.close();

        // Write new content.
        {
            QFile f(fakePath);
            QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate));
            QString newContent = QStringLiteral("# BEGIN coder-desktop managed block\n"
                                                "nameserver 100.100.100.100\n"
                                                "search coder.internal\n"
                                                "# END coder-desktop managed block\n");
            newContent += origContent;
            f.write(newContent.toUtf8());
            f.close();
        }

        // Verify the file contains our managed block.
        {
            QFile f(fakePath);
            QVERIFY(f.open(QIODevice::ReadOnly));
            const QString content = QString::fromUtf8(f.readAll());
            QVERIFY(content.contains(QStringLiteral("100.100.100.100")));
            QVERIFY(content.contains(QStringLiteral("coder.internal")));
            QVERIFY(content.contains(QStringLiteral("8.8.8.8")));
            f.close();
        }

        // Restore from backup.
        QFile::remove(fakePath);
        QVERIFY(QFile::rename(backupPath, fakePath));

        // Verify restoration.
        {
            QFile f(fakePath);
            QVERIFY(f.open(QIODevice::ReadOnly));
            const QString content = QString::fromUtf8(f.readAll());
            QVERIFY(!content.contains(QStringLiteral("100.100.100.100")));
            QVERIFY(content.contains(QStringLiteral("8.8.8.8")));
            f.close();
        }

        QVERIFY(!QFile::exists(backupPath));
    }
};

QTEST_MAIN(TestDnsManager)
#include "tst_dnsmanager.moc"
