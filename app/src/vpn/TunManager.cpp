#include "vpn/TunManager.h"

#include <QDebug>

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>

#include <linux/if.h>
#include <linux/if_tun.h>
#include <sys/ioctl.h>

int TunManager::createTun(const QString& name)
{
    int fd = ::open("/dev/net/tun", O_RDWR | O_CLOEXEC);
    if (fd < 0) {
        qWarning() << "TunManager: failed to open /dev/net/tun:"
                    << strerror(errno);
        return -1;
    }

    struct ifreq ifr {};
    ifr.ifr_flags = IFF_TUN | IFF_NO_PI;

    const QByteArray nameUtf8 = name.toUtf8();
    qstrncpy(ifr.ifr_name, nameUtf8.constData(), IFNAMSIZ);

    if (::ioctl(fd, TUNSETIFF, &ifr) < 0) {
        qWarning() << "TunManager: TUNSETIFF failed for" << name
                    << ":" << strerror(errno);
        ::close(fd);
        return -1;
    }

    qInfo() << "TunManager: created TUN device" << ifr.ifr_name
            << "fd=" << fd;
    return fd;
}

void TunManager::destroyTun(int fd)
{
    if (fd >= 0)
        ::close(fd);
}

void TunManager::addRoute(const QString& cidr, const QString& dev)
{
    // TODO: implement via netlink (Phase 2).
    Q_UNUSED(cidr)
    Q_UNUSED(dev)
    qDebug() << "TunManager::addRoute stub —" << cidr << "via" << dev;
}

void TunManager::removeRoute(const QString& cidr, const QString& dev)
{
    // TODO: implement via netlink (Phase 2).
    Q_UNUSED(cidr)
    Q_UNUSED(dev)
    qDebug() << "TunManager::removeRoute stub —" << cidr << "via" << dev;
}
