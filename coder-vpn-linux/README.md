# coder-vpn-linux

Go shared library (`libcodervpn.so`) that exposes the Coder tailnet VPN stack
to a native Linux desktop application via a C API.

This module mirrors the Android gomobile bridge (`coder-vpn-android`) but uses
`cgo //export` functions and C function-pointer callbacks instead of Java
interfaces.

## Building

```bash
# Build the shared library (produces libcodervpn.so + libcodervpn.h)
./scripts/build-so.sh

# Or manually:
CGO_ENABLED=1 go build -buildmode=c-shared -o libcodervpn.so .
```

Requires Go 1.24+ and a C compiler (gcc/clang).

## C API

The generated `libcodervpn.h` header declares these functions:

### `CoderVPN_Start`
```c
int32_t CoderVPN_Start(
    const char* coderURL,
    const char* apiToken,
    network_settings_cb onNetworkSettings,
    peer_update_cb      onPeerUpdate,
    error_cb            onError,
    log_cb              onLog
);
```
Starts the VPN tunnel. Authenticates with the Coder deployment, computes
network settings, calls `onNetworkSettings` to get a TUN file descriptor,
then runs the tailnet connection in background goroutines.

Returns 0 on success, -1 on error.

### `CoderVPN_Stop`
```c
int32_t CoderVPN_Stop(void);
```
Tears down the VPN tunnel and releases all resources. Returns 0.

### `CoderVPN_IsRunning`
```c
int32_t CoderVPN_IsRunning(void);
```
Returns 1 if the tunnel is active, 0 otherwise.

### `CoderVPN_UpdateTunFd`
```c
int32_t CoderVPN_UpdateTunFd(int32_t newFd);
```
Replaces the TUN file descriptor (e.g. after network change). Returns 0 on
success, -1 on error.

## Callback Types

```c
// Called when Go needs network settings applied. Return the new TUN fd, or -1.
typedef int32_t (*network_settings_cb)(
    const char* addresses,       // comma-separated CIDR
    const char* dns_servers,     // comma-separated IPs
    const char* search_domains,  // comma-separated
    const char* routes,          // comma-separated CIDR
    int32_t mtu
);

// Called when a workspace agent peer changes state.
typedef void (*peer_update_cb)(
    const char* workspace_name,
    const char* agent_name,
    const char* hostname,
    int32_t status,              // 0=DISCONNECTED, 1=CONNECTING, 2=CONNECTED
    int64_t last_ping_ms,
    int32_t is_p2p               // 0=relay, 1=direct
);

// Called on fatal tunnel errors after Start returns.
typedef void (*error_cb)(const char* message);

// Called for log messages. level: 0=DEBUG, 1=INFO, 2=WARN, 3=ERROR
typedef void (*log_cb)(int32_t level, const char* message);
```

## Dependencies

This module pins `github.com/coder/coder/v2 v2.30.1` and uses the same
`replace` directives as the Android bridge (Coder forks of tailscale,
wireguard-go, golang-x-crypto, gvisor).
