#ifndef TUNMANAGER_H
#define TUNMANAGER_H

#include <QString>

#include "util/UniqueFd.h"

/// Linux TUN device management for the Coder VPN tunnel.
///
/// Uses /dev/net/tun with IFF_TUN | IFF_NO_PI to create a layer-3 tunnel
/// device.  Route manipulation is stubbed — full netlink implementation
/// is planned for Phase 2.
class TunManager {
public:
    TunManager() = default;

    /// Create a TUN device with the given name.
    /// @return RAII-wrapped file descriptor (invalid on error).
    [[nodiscard]] UniqueFd createTun(const QString& name);

    /// Add a route via the TUN device (stub — TODO: netlink).
    static void addRoute(const QString& cidr, const QString& dev);

    /// Remove a route via the TUN device (stub — TODO: netlink).
    static void removeRoute(const QString& cidr, const QString& dev);
};

#endif // TUNMANAGER_H
