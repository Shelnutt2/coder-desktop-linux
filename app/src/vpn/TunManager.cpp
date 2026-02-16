#include "vpn/TunManager.h"

#include <QDebug>

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>

#include <linux/if.h>
#include <linux/if_tun.h>
#include <sys/ioctl.h>

UniqueFd TunManager::createTun(const QString& name)
{
    UniqueFd fd(::open("/dev/net/tun", O_RDWR | O_CLOEXEC));
    if (!fd) {
        qWarning() << "TunManager: failed to open /dev/net/tun:"
                    << strerror(errno);
        return {};
    }

    struct ifreq ifr {};
    ifr.ifr_flags = IFF_TUN | IFF_NO_PI;

    const QByteArray nameUtf8 = name.toUtf8();
    qstrncpy(ifr.ifr_name, nameUtf8.constData(), IFNAMSIZ);

    if (::ioctl(fd.get(), TUNSETIFF, &ifr) < 0) {
        qWarning() << "TunManager: TUNSETIFF failed for" << name
                    << ":" << strerror(errno);
        return {};
    }

    qInfo() << "TunManager: created TUN device" << ifr.ifr_name
            << "fd=" << fd.get();
    return fd;
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
