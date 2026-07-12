# Coder Desktop for Linux — Architecture

This document describes the runtime architecture of Coder Desktop for Linux,
including the component model, inter-process communication via D-Bus, security
boundaries, and data flow.

## Component Overview

Coder Desktop for Linux is a monorepo producing three build targets:

| Component | Binary / Library | Language | Runs As | Description |
|-----------|-----------------|----------|---------|-------------|
| Desktop App | `coder-desktop` | C++20 / QML | User | Qt 6 system-tray application — UI, workspace management, settings |
| VPN Helper | `coder-desktop-helper` | Go | Root | Privileged D-Bus system service — TUN device, DNS, routing, VPN tunnel |
| DLP Compositor | `libcoderdlp.so` | C | User (in-process) | wlroots-based nested Wayland compositor for Data Loss Prevention |

```
┌─────────────────────────────────────────────────────────────────────┐
│  User Session                                                       │
│                                                                     │
│  ┌─────────────────────────────────────┐                            │
│  │  coder-desktop  (Qt 6 / C++20)     │                            │
│  │                                     │                            │
│  │  ┌───────────┐  ┌───────────────┐  │   HTTPS                    │
│  │  │  QML UI   │  │ libcoderdlp.so│  │ ──────────► Coder API      │
│  │  │  (tray,   │  │ (DLP composi- │  │                            │
│  │  │  pages)   │  │  tor — Wl/X11 │  │                            │
│  │  │           │  │  + Xwayland)  │  │                            │
│  │  └───────────┘  └───────────────┘  │                            │
│  │           │                         │                            │
│  └───────────┼─────────────────────────┘                            │
│              │ D-Bus system bus                                      │
│              │ com.coder.Desktop.Helper1                            │
│              │ (Start / Stop / GetStatus / signals)                 │
└──────────────┼──────────────────────────────────────────────────────┘
               │
┌──────────────┼──────────────────────────────────────────────────────┐
│  System (root)                                                      │
│              ▼                                                       │
│  ┌─────────────────────────────────────┐                            │
│  │  coder-desktop-helper  (Go)        │                            │
│  │                                     │                            │
│  │  ┌──────────────────────────────┐  │                            │
│  │  │  D-Bus Service               │  │  WireGuard                  │
│  │  │  com.coder.Desktop.Helper1   │  │ ──────────► Coder Deploy.  │
│  │  │                              │  │  (tailscale tunnel)         │
│  │  │  ┌─────────┐ ┌───────────┐  │  │                            │
│  │  │  │TUN mgmt │ │DNS config │  │  │                            │
│  │  │  │(coder0) │ │(resolvconf│  │  │                            │
│  │  │  │         │ │/resolvectl│  │  │                            │
│  │  │  └─────────┘ │/resolv.cf)│  │  │                            │
│  │  │              └───────────┘  │  │                            │
│  │  └──────────────────────────────┘  │                            │
│  └─────────────────────────────────────┘                            │
└─────────────────────────────────────────────────────────────────────┘
```

## D-Bus System Service — `coder-desktop-helper`

The VPN helper is the core privileged component. It runs as root because
managing TUN devices and system routes requires `CAP_NET_ADMIN`.

### Why D-Bus?

The previous design considered embedding VPN logic as a Go shared library
(`libcodervpn.so`) loaded via `dlopen()` into the Qt process. The current
architecture uses a separate privileged binary communicating over D-Bus because:

1. **Privilege separation** — The VPN helper needs root; the UI should not.
   D-Bus + Polkit provides clean privilege escalation without running the
   entire application as root.
2. **Process isolation** — A Go runtime embedded via cgo in a Qt/C++ process
   leads to complex interactions (two GCs, signal handling conflicts, stack
   management). A separate process avoids these issues entirely.
3. **System integration** — D-Bus activation and systemd service management are
   the standard Linux patterns for system services. The helper auto-starts on
   first method call and can be managed with `systemctl`.
4. **Crash isolation** — If the VPN tunnel crashes, only the helper restarts.
   The UI remains responsive and can reconnect.

### Interface Definition

The D-Bus interface is defined in [`dbus/com.coder.Desktop.Helper1.xml`](../dbus/com.coder.Desktop.Helper1.xml):

- **Bus name:** `com.coder.Desktop.Helper`
- **Object path:** `/com/coder/Desktop/Helper`
- **Interface:** `com.coder.Desktop.Helper1`

#### Methods

| Method | Arguments | Returns | Description |
|--------|-----------|---------|-------------|
| `Start` | `coder_url: string`, `api_token: string` | — | Connect to a Coder deployment and establish the VPN tunnel |
| `Stop` | — | — | Tear down the active VPN tunnel |
| `GetStatus` | — | `state: string`, `coder_url: string` | Query the current VPN state |

**VPN States:** `disconnected` → `connecting` → `connected` → `disconnecting` → `disconnected`

#### Signals

| Signal | Arguments | Description |
|--------|-----------|-------------|
| `StateChanged` | `new_state: string`, `error_message: string` | Emitted on every VPN state transition. `error_message` is non-empty when the transition was caused by an error. |
| `PeerUpdated` | `workspace: string`, `agent: string`, `hostname: string`, `status: int`, `last_ping_ms: int`, `is_p2p: bool` | Emitted when a workspace peer is added, updated, or removed. The Qt app uses this to populate the workspace list. |
| `LogMessage` | `level: int`, `message: string` | Forwarded log lines from the helper (0=DEBUG, 1=INFO, 2=WARN, 3=ERROR). |

### Activation & Lifecycle

The helper can be started in two ways:

1. **D-Bus activation** — When the Qt app calls any method on `com.coder.Desktop.Helper`, the D-Bus daemon auto-starts the helper using the service file at `/usr/share/dbus-1/system-services/com.coder.Desktop.Helper.service`.

2. **systemd service** — `systemctl start coder-desktop-helper.service` starts the helper directly. The systemd unit is configured as `Type=dbus` so systemd considers it ready once it acquires the bus name.

The helper exits gracefully when the bus connection is lost or when stopped via systemd.

### Configuration Files

| File | Location | Purpose |
|------|----------|---------|
| D-Bus interface XML | [`dbus/com.coder.Desktop.Helper1.xml`](../dbus/com.coder.Desktop.Helper1.xml) | Introspection data — defines methods and signals |
| D-Bus bus config | [`dbus/com.coder.Desktop.Helper.conf`](../dbus/com.coder.Desktop.Helper.conf) | Allows root to own the bus name; allows all users to call methods |
| D-Bus service file | [`dbus/com.coder.Desktop.Helper.service.in`](../dbus/com.coder.Desktop.Helper.service.in) | D-Bus activation entry (template — `@HELPER_BINARY_PATH@` substituted at install) |
| systemd unit | [`packaging/systemd/coder-desktop-helper.service.in`](../packaging/systemd/coder-desktop-helper.service.in) | systemd service with security hardening |
| Polkit policy | [`packaging/polkit/com.coder.Desktop.Helper.policy`](../packaging/polkit/com.coder.Desktop.Helper.policy) | Authorization rules for VPN management |

## Security Model

### Polkit Authorization

The Polkit policy defines two actions:

| Action | Default (active session) | Default (inactive/remote) | Description |
|--------|-------------------------|---------------------------|-------------|
| `com.coder.Desktop.Helper.manage-vpn` | `yes` (no prompt) | `auth_admin` | Start/Stop the VPN tunnel |
| `com.coder.Desktop.Helper.status` | `yes` | `yes` | Query VPN status (read-only) |

This matches the NetworkManager pattern: users logged into a local desktop
session can manage the VPN without a password prompt, while remote/SSH sessions
require administrator authentication.

### systemd Hardening

The helper's systemd unit applies security restrictions:

- `CapabilityBoundingSet=CAP_NET_ADMIN CAP_NET_RAW` — Only the capabilities needed for TUN/route management
- `ProtectSystem=strict` — Read-only filesystem except for allowed paths
- `ProtectHome=yes` — No access to user home directories
- `PrivateTmp=yes` — Isolated `/tmp` namespace

### Credential Flow

API tokens never persist in the helper. The Qt app retrieves the token from
`libsecret` (GNOME Keyring / KWallet) and passes it to the helper via the
`Start()` D-Bus method call. The helper holds the token in memory only for
the lifetime of the VPN session.

```
User enters credentials in Qt app
        │
        ▼
Qt app stores token via libsecret
        │
        ▼
Qt app calls Helper.Start(url, token) over D-Bus
        │
        ▼
Helper holds token in memory, establishes tunnel
        │
        ▼
On Stop() or exit, token is discarded
```

## VPN Helper Internals

### Go Module Structure

The helper lives in `coder-vpn-linux/` and uses these key dependencies:

| Dependency | Purpose |
|------------|---------|
| `github.com/coder/coder/v2` | Coder SDK — VPN tunnel management, workspace discovery |
| `tailscale.com` | Tailscale client — WireGuard tunnel, DERP relay |
| `github.com/tailscale/wireguard-go` | WireGuard implementation |
| `github.com/godbus/dbus/v5` | D-Bus protocol bindings for Go |
| `github.com/mdlayher/sdnotify` | systemd readiness notification (`sd_notify`) |

### Internal Packages

```
coder-vpn-linux/
├── cmd/coder-desktop-helper/
│   └── main.go              # Entry point — D-Bus registration, signal handling
└── internal/
    ├── dbusservice/          # D-Bus method/signal implementation
    ├── dns/                  # DNS configuration (resolvconf → resolvectl → resolv.conf)
    └── sdutil/               # systemd sd_notify wrapper
```

### DNS Configuration

The helper configures DNS for the VPN tunnel using a cascade strategy
(matching `wg-quick`):

1. **`resolvconf`** (preferred) — Universal DNS abstraction. On systemd systems,
   this is often a symlink to `resolvectl`. On non-systemd systems (Alpine,
   Void, Gentoo), `openresolv` handles it.
2. **`resolvectl`** — Direct systemd-resolved control. Used when `resolvconf`
   is not available.
3. **`/etc/resolv.conf` fallback** — Direct file edit with backup/restore. Used
   on minimal systems with neither `resolvconf` nor `resolvectl`.

## Desktop Application — `coder-desktop`

The Qt 6 application runs as the logged-in user and provides:

- **System tray** icon with quick VPN connect/disconnect
- **Workspace browser** — list, start, stop, monitor workspaces
- **Coder Agents** — chat with AI agents: streaming responses, live status, and notifications
- **Settings UI** — deployment URLs, VPN preferences, DLP toggles
- **DLP compositor** — launches `libcoderdlp.so` in-process when enabled

### Qt/D-Bus Integration

The app uses Qt's D-Bus module (`QDBusInterface` / `QDBusConnection`) to
communicate with the helper:

1. On startup, connect to `com.coder.Desktop.Helper` on the system bus
2. Subscribe to `StateChanged`, `PeerUpdated`, and `LogMessage` signals
3. Call `GetStatus()` to sync initial state
4. User actions (connect/disconnect) trigger `Start()` / `Stop()` method calls

If the helper is not running, the first D-Bus method call triggers automatic
activation via the D-Bus service file.

### Three-Layer Settings

Settings resolve through three layers (highest priority first):

1. **MDM policy** — `/etc/coder-desktop/policy.json` (read-only, can lock settings)
2. **User preferences** — `~/.config/coder-desktop/settings.json`
3. **Compiled defaults**

All settings access goes through `SettingsManager` which merges these layers.
MDM policy can both set values and lock them (preventing user override).

## DLP Compositor — `libcoderdlp.so`

The DLP compositor is a wlroots-based nested compositor loaded in-process by
the Qt app via `dlopen()`. It creates an isolated rendering environment where
workspace applications run under enforced security policies.

### Enforcement Capabilities

| Policy | Mechanism | Notes |
|--------|-----------|-------|
| **Clipboard isolation** | Wayland protocol mediation in `clipboard.c` | Blocks data transfer between sandbox and host |
| **Screenshot prevention** | No `wlr_screencopy` / `wlr_export_dmabuf` protocols | Host screen capture tools cannot reach inside the compositor |
| **File sandbox** | bubblewrap (`--bind`, `--ro-bind`, `--tmpfs`) | Restricts filesystem access to allowed paths |
| **Network sandbox** | bubblewrap `--unshare-net` | Optional network namespace isolation |
| **D-Bus filtering** | `xdg-dbus-proxy` with `--talk` allowlist | Restricts which D-Bus services sandboxed apps can access |
| **Steganographic watermarking** | Per-frame invisible watermark in `watermark.c` | Encodes user identity fingerprint for forensic tracing |
| **Wayland security context** | `wlr_security_context_v1` in `security_context.c` | Marks sandbox clients for host compositor policy |

### Host Desktop Support

The compositor supports both Wayland and X11 host desktops via wlroots
backend auto-detection:

| Host Session | wlroots Backend | Compositor Window | Security Level |
|-------------|----------------|-------------------|---------------|
| **Wayland** | `wlr_wl_backend` | Nested Wayland toplevel | 🟢 Full — Wayland protocol enforces per-client isolation |
| **X11** | `wlr_x11_backend` | Regular X11 window | 🟡 Reduced — policies enforced within sandbox, but host X11 cannot prevent external screen capture |

The Qt app detects the backend type and displays a security level indicator
(green badge for Wayland, yellow for X11) so users understand the protection
tier.

On X11 hosts, the compositor opens as a regular X11 window. Input from the
host X11 server is forwarded to sandboxed applications inside it. The Super
(Mod4) key is filtered on the X11 backend to prevent leaked host window
manager keybindings from reaching sandboxed apps.

### Xwayland Support

X11-only applications (JetBrains IDEs, legacy GTK2 apps, some Electron apps)
run inside the sandbox via Xwayland:

```
┌──────────────────────────────────────────────┐
│  DLP Compositor (libcoderdlp.so)             │
│                                              │
│  ┌──────────────┐  ┌─────────────────────┐  │
│  │ Native       │  │ Xwayland            │  │
│  │ Wayland apps │  │  ┌───────────────┐  │  │
│  │ (Firefox,    │  │  │ X11 apps      │  │  │
│  │  terminals)  │  │  │ (VSCode,      │  │  │
│  │              │  │  │  CLion, etc.)  │  │  │
│  └──────────────┘  │  └───────────────┘  │  │
│                    └─────────────────────┘  │
└──────────────────────────────────────────────┘
```

Xwayland is started in lazy mode — it only spawns when an X11 client connects.
`XWAYLAND_NO_GLAMOR=1` is set in the compositor environment before Xwayland
creation to disable glamor/DRI3, forcing `wl_shm` buffer transport. This avoids
DMA-BUF modifier incompatibility (e.g., Intel CCS tiling) between Xwayland and
the nested compositor's renderer while preserving GPU rasterization in X11
clients.

When both `DISPLAY` and `WAYLAND_DISPLAY` are available inside the sandbox,
environment hints (`ELECTRON_OZONE_PLATFORM_HINT=wayland`,
`MOZ_ENABLE_WAYLAND=1`) nudge Electron and Firefox to prefer native Wayland,
which provides better performance and avoids the Xwayland code path entirely.

### Sandbox Launcher

Applications are launched inside the DLP compositor via bubblewrap (`bwrap`):

```
coder-desktop (Qt app)
  └─ libcoderdlp.so (in-process)
       ├─ wlroots backend (Wayland or X11 connection to host)
       ├─ Xwayland (lazy, if X11 apps connect)
       ├─ xdg-dbus-proxy (filtered D-Bus socket)
       └─ bwrap --new-session --die-with-parent
            └─ sandboxed application
```

Key sandbox features:
- **Process group isolation** — Fork children call `setpgid(0, 0)` to prevent
  terminal signals (Ctrl+C) from reaching them directly
- **`--new-session`** — bwrap creates a new terminal session (`setsid()`) for
  the sandboxed app, isolating it from the host terminal
- **`--die-with-parent`** — bwrap uses `PR_SET_PDEATHSIG` to ensure sandboxed
  apps are cleaned up when the compositor exits
- **D-Bus proxy lifecycle** — Managed via a pipe fd; closing the write end
  signals the proxy to exit. Zombie reaping runs on a 30-second timer.
- **Per-PID log files** — Child process output is captured to
  `/tmp/coder-dlp-child-<pid>.log` with `O_NOFOLLOW` for symlink safety

### Event Loop Integration

The compositor uses wlroots' `wl_event_loop`. The Qt app integrates it using
`QSocketNotifier` on the Wayland display file descriptor, dispatching wlroots
events from the Qt main thread. The compositor is **not** thread-safe and must
**not** run on a separate thread.

### Graceful Shutdown

The Qt app installs `SIGINT`/`SIGTERM` handlers that bridge to
`QApplication::quit()` via a `socketpair` + `QSocketNotifier`. This ensures
C++ destructors run on Ctrl+C, allowing `DlpCompositorManager::stopAll()` to
properly clean up:

1. Send `SIGTERM` to tracked sandbox PIDs
2. Destroy `QSocketNotifier` event loop integrations
3. Call `coder_dlp_destroy()` for each compositor, which:
   - Cleans up D-Bus proxy processes
   - Destroys Xwayland (`wlr_xwayland_destroy`)
   - Releases wlroots backend (`wlr_backend_destroy`) — closes host connection
   - Frees GPU resources (GBM buffers, EGL contexts)

## Data Flow Summary

```
┌──────────┐   HTTPS/REST   ┌──────────────────┐
│  Qt App  │ ──────────────► │  Coder Deployment │
│          │                 │  (REST API)       │
│          │   D-Bus calls   └──────────────────┘
│          │ ──────────────►          ▲
│          │                          │
│          │   D-Bus signals ┌────────┴─────────┐
│          │ ◄────────────── │  coder-desktop-  │
│          │                 │  helper           │
│          │                 │                   │
│          │   dlopen()      │  WireGuard tunnel │
│          │ ──► libcoderdlp │  (tailscale)      │
└──────────┘    .so (DLP)    └──────────────────┘
```

| Path | Protocol | Direction | Purpose |
|------|----------|-----------|---------|
| App → Helper | D-Bus system bus | Method calls | `Start`, `Stop`, `GetStatus` |
| Helper → App | D-Bus system bus | Signals | `StateChanged`, `PeerUpdated`, `LogMessage` |
| App → Coder API | HTTPS/REST | Request/Response | Workspace listing, build logs, agent chats |
| Helper → Coder | WireGuard (UDP) | Bidirectional | VPN tunnel to workspaces via tailscale |
| App → DLP lib | `dlopen()` / C API | In-process calls | Compositor lifecycle management |
| DLP → Xwayland | Wayland protocol | In-process fork | X11 app compatibility layer |
| DLP → bwrap | fork/exec | Child process | Sandboxed app launcher |
| DLP → xdg-dbus-proxy | fork/exec + pipe | Child process | Filtered D-Bus socket |

## Build Targets

```bash
# Full build (all targets)
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)

# Go helper binary (built separately from CMake)
cd coder-vpn-linux && go build -o ../build/coder-desktop-helper ./cmd/coder-desktop-helper/

# DLP compositor library
cmake --build build --target coderdlp

# Qt desktop application
cmake --build build --target coder-desktop
```
