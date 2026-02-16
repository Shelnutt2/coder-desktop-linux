# AGENTS.md — Coder Desktop for Linux

## Project Overview

Coder Desktop for Linux is a monorepo producing three build targets:

1. **`libcodervpn.so`** — Go c-shared library wrapping `github.com/coder/coder`'s VPN/tunnel SDK. Built with `go build -buildmode=c-shared`. Lives in `coder-vpn-linux/`.
2. **`libcoderdlp.so`** — C library implementing a wlroots-based nested Wayland compositor for Data Loss Prevention. Lives in `coder-dlp-compositor/`.
3. **`coder-desktop`** — Qt 6 / C++ desktop application (system tray, workspace management, VPN control, settings). Lives in `app/`.

CMake is the top-level build system. The Go library is built via a custom command; the other two targets use standard CMake `add_subdirectory()`.

## Directory Structure

```
coder-desktop-linux/
├── CMakeLists.txt              # Top-level CMake (3 targets)
├── AGENTS.md                   # This file
├── README.md
├── LICENSE
├── docs/
│   └── planned_implementation.md
├── coder-vpn-linux/            # Go module → libcodervpn.so
│   ├── go.mod / go.sum
│   ├── bridge.go               # //export functions (C API)
│   ├── internal/               # Go internals (tunnel, auth, DNS)
│   └── scripts/build-so.sh     # Build script invoked by CMake
├── coder-dlp-compositor/       # C / wlroots → libcoderdlp.so
│   ├── CMakeLists.txt
│   ├── include/coderdlp.h      # Public C API
│   └── src/                    # Compositor implementation
├── app/                        # Qt 6 / C++ → coder-desktop
│   ├── CMakeLists.txt
│   ├── src/                    # C++ sources
│   ├── qml/                    # QML UI files
│   └── tests/                  # Unit tests
├── packaging/                  # Distribution packaging
│   ├── deb/ rpm/ flatpak/ appimage/
└── .github/workflows/          # CI pipelines
```

## Build Commands

### Full build (all targets)

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
```

### Individual targets

```bash
# Go VPN shared library only
cmake --build build --target codervpn_so

# DLP compositor only
cmake --build build --target coderdlp

# Qt app only
cmake --build build --target coder-desktop
```

### Go library standalone (without CMake)

```bash
cd coder-vpn-linux
./scripts/build-so.sh /tmp/libcodervpn.so
```

## Architecture Cheat Sheet

| Component | Language | Build | Key Dependencies |
|-----------|----------|-------|------------------|
| `coder-vpn-linux/` | Go | `go build -buildmode=c-shared` | `coder/coder` SDK, `tailscale` |
| `coder-dlp-compositor/` | C | CMake / pkg-config | `wlroots 0.19`, `wayland`, `xkbcommon` |
| `app/` | C++ / QML | CMake / Qt 6 | Qt 6.5+ (Widgets, Quick, Network, WebEngine), `libsecret` |

### Data flow

```
coder-desktop (Qt app)
  ├── dlopen(libcodervpn.so) → calls Go VPN functions via C ABI
  ├── dlopen(libcoderdlp.so) → launches nested Wayland compositor
  └── REST → Coder deployment API
```

## Key Patterns

### C callbacks from Go shared library

The Go `.so` exports C functions. The Qt app calls them and registers C callback
function pointers for async events (VPN state changes, peer updates, log messages).
All callbacks must be safe to call from any goroutine — they post events to the
Qt event loop via `QMetaObject::invokeMethod(obj, Qt::QueuedConnection, ...)`.

### QSocketNotifier for wlroots event loop integration

The DLP compositor runs a wlroots `wl_event_loop`. The Qt app integrates it
using `QSocketNotifier` on the Wayland display fd, dispatching wlroots events
from the Qt event loop. Do NOT spin up a separate thread for the compositor
event loop.

### Three-layer settings

Settings resolve through three layers (highest priority first):
1. **MDM policy** — `/etc/coder-desktop/policy.json` (read-only, can lock settings)
2. **User preferences** — `~/.config/coder-desktop/settings.json` (QSettings)
3. **Compiled defaults**

When MDM is not present, all settings appear as editable user preferences.

### Credential storage

Secrets (API tokens) go through `libsecret` (D-Bus Secret Service API), NOT
in settings files. Fallback: encrypted file for headless/no-keyring environments.

## Testing

```bash
# Qt app unit tests
cd build && ctest --test-dir app --output-on-failure

# Go tests
cd coder-vpn-linux && go test ./...

# DLP compositor tests (if any)
cd build && ctest --test-dir coder-dlp-compositor --output-on-failure
```

## What NOT To Do

1. **Do NOT modify `coder/coder`** — The upstream Coder repository is a dependency, not part of this repo. Any VPN/tunnel SDK changes must be proposed upstream.

2. **Do NOT use X11 for DLP** — The DLP compositor MUST use Wayland (wlroots). X11 cannot enforce clipboard/screenshot restrictions because any client can read any other client's windows. This is a fundamental security requirement.

3. **Only one Go c-shared `.so` per process** — The Go runtime can only be initialized once per process. Do NOT try to load multiple Go shared libraries. Everything that needs Go goes into `libcodervpn.so`.

4. **Do NOT run the wlroots event loop on a separate thread** — Integrate via `QSocketNotifier` on the Qt main thread. wlroots is not thread-safe.

5. **Do NOT store secrets in settings files** — Use `libsecret` for API tokens and session credentials. Settings files are plaintext JSON.

6. **Do NOT bypass the settings layer resolution** — Always go through `SettingsManager` to read settings. Never read user prefs or MDM policy directly in UI code.
