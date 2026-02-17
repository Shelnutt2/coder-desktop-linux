# AGENTS.md — Coder Desktop for Linux

## Project Overview

Coder Desktop for Linux is a monorepo producing three build targets:

1. **`coder-desktop-helper`** — Go binary that runs as a privileged D-Bus system service, managing the VPN tunnel (TUN creation, DNS, routing). Communicates with the Qt app over D-Bus. Lives in `coder-vpn-linux/`.
2. **`libcoderdlp.so`** — C library implementing a wlroots-based nested Wayland compositor for Data Loss Prevention. Lives in `coder-dlp-compositor/`.
3. **`coder-desktop`** — Qt 6 / C++ desktop application (system tray, workspace management, VPN control, settings). Lives in `app/`.

CMake is the top-level build system. The Go helper binary is built separately; the other two targets use standard CMake `add_subdirectory()`.

## Directory Structure

```
coder-desktop-linux/
├── CMakeLists.txt              # Top-level CMake (3 targets)
├── AGENTS.md                   # This file
├── README.md
├── LICENSE
├── docs/
│   └── planned_implementation.md
├── coder-vpn-linux/            # Go module → coder-desktop-helper
│   ├── go.mod / go.sum
│   ├── cmd/coder-desktop-helper/main.go  # Entry point
│   └── internal/
│       ├── dbusservice/        # D-Bus service implementation
│       ├── dns/                # DNS configuration
│       └── sdutil/             # systemd notify wrapper
├── coder-dlp-compositor/       # C / wlroots → libcoderdlp.so
│   ├── CMakeLists.txt
│   ├── include/coderdlp.h      # Public C API
│   └── src/                    # Compositor implementation
├── app/                        # Qt 6 / C++ → coder-desktop
│   ├── CMakeLists.txt
│   ├── src/                    # C++ sources
│   ├── qml/                    # QML UI files
│   └── tests/                  # Unit tests
├── dbus/                       # D-Bus config, interface XML, service files
├── packaging/                  # Distribution packaging
│   ├── deb/ rpm/ flatpak/ appimage/
│   ├── polkit/                 # Polkit policy for privileged helper
│   └── systemd/                # systemd service unit for helper
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
# Go helper binary only
cd coder-vpn-linux && go build -o ../build/coder-desktop-helper ./cmd/coder-desktop-helper/

# DLP compositor only
cmake --build build --target coderdlp

# Qt app only
cmake --build build --target coder-desktop
```

### Go helper standalone (without CMake)

```bash
cd coder-vpn-linux && go build ./cmd/coder-desktop-helper/
```

## Formatting

All C, C++, and header files under `app/` and `coder-dlp-compositor/` are formatted with **clang-format**. The style is defined in `.clang-format` at the repo root. CI enforces formatting via `clang-format --dry-run --Werror`.

### Format all files

```bash
find app/src coder-dlp-compositor/src -type f \( -name '*.h' -o -name '*.cpp' -o -name '*.c' \) \
  -exec clang-format -i {} +
```

### Pre-commit hook

Install the git pre-commit hook to catch formatting issues before commit:

```bash
./scripts/install-hooks.sh
```

### Go

Go code in `coder-vpn-linux/` follows standard `gofmt` formatting.

## Architecture Cheat Sheet

| Component | Language | Build | Key Dependencies |
|-----------|----------|-------|------------------|
| `coder-vpn-linux/` | Go | `go build` | `coder/coder` SDK, `tailscale`, `godbus/dbus/v5` |
| `coder-dlp-compositor/` | C | CMake / pkg-config | `wlroots 0.19`, `wayland`, `xkbcommon` |
| `app/` | C++ / QML | CMake / Qt 6 | Qt 6.5+ (Widgets, Quick, Network, WebEngine), `libsecret` |

### Data flow

```
coder-desktop-helper (Go, runs as root)
  ├── D-Bus system bus ← com.coder.Desktop.Helper1
  ├── TUN device, DNS, routing
  └── coder/coder VPN SDK → tailscale tunnel

coder-desktop (Qt app, runs as user)
  ├── D-Bus → coder-desktop-helper (Start/Stop VPN, receive state signals)
  ├── dlopen(libcoderdlp.so) → launches nested Wayland compositor
  └── REST → Coder deployment API
```

## C++ Coding Standards

The Qt application (`app/`) targets **C++20 or newer** and follows the [C++ Core Guidelines](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines) where practical.

### RAII & Smart Pointers

- **Prefer RAII for all resource management.** Every resource (memory, file descriptors, sockets, network connections) should be owned by an object whose destructor releases it.
- **Use `std::unique_ptr` or `std::shared_ptr`** for owning pointers. Avoid bare `new` / `delete`.
  - **Exception — Qt parent ownership:** `new QFoo(parent)` is acceptable because Qt's parent-child tree IS RAII: the parent's destructor deletes all children. Prefer this idiom for QObject-derived types that participate in the object tree.
- **Non-owning pointers:** Raw pointers (`T*`) are allowed ONLY as non-owning observers/references (e.g., injected dependencies whose lifetime is guaranteed by the caller). Document these with a comment: `// non-owning`. Prefer `T&` (references) when null is never valid.
- **File descriptors:** Wrap in a RAII class (e.g., `UniqueFd`) instead of passing raw `int` around with manual `close()` calls.
- **Use `[[nodiscard]]` on functions where ignoring the return value is likely a bug** (factory functions, error codes, state queries).

### Modern C++ Practices

- Prefer `auto` with initializers for complex types, but be explicit for readability where the type isn't obvious.
- Use `constexpr` and `const` liberally.
- Use `enum class` (not unscoped `enum`) for all new enumerations.
- Prefer range-based `for` loops.
- Prefer `std::string_view`, `QStringView`, or `QLatin1StringView` for non-owning string parameters.
- Use structured bindings (`auto [key, value] = ...`) where it improves clarity.
- Use `= default` / `= delete` for special member functions.

## Key Patterns

### D-Bus system service (privileged helper)

The `coder-desktop-helper` binary runs as root via D-Bus system bus activation
(or a systemd service). It exposes the `com.coder.Desktop.Helper1` interface.
The Qt app sends method calls (`Start`, `Stop`, `GetStatus`) and subscribes to
signals (`StateChanged`, `PeerUpdated`, `LogMessage`). Polkit authorizes
privileged operations so the unprivileged Qt app can control the VPN.
D-Bus activation auto-starts the helper on first method call if it isn't already
running.

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

3. **Do NOT run the helper as an unprivileged user** — It requires `CAP_NET_ADMIN` for TUN/route management and runs as root via D-Bus activation. Attempting to run it unprivileged will fail to create TUN devices and manage routes.

4. **Do NOT run the wlroots event loop on a separate thread** — Integrate via `QSocketNotifier` on the Qt main thread. wlroots is not thread-safe.

5. **Do NOT store secrets in settings files** — Use `libsecret` for API tokens and session credentials. Settings files are plaintext JSON.

6. **Do NOT bypass the settings layer resolution** — Always go through `SettingsManager` to read settings. Never read user prefs or MDM policy directly in UI code.
