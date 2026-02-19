You are an experienced, pragmatic software engineering AI agent. Do not over-engineer a solution when a simple one is possible. Keep edits minimal. If you want an exception to ANY rule, you MUST stop and get permission first.

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

## Reference

### Important Files

| File | Purpose |
|------|---------|
| `CMakeLists.txt` | Top-level CMake config defining all three build targets |
| `.clang-format` | clang-format style (Google-based, 100-col, 4-space indent) |
| `app/CMakeLists.txt` | Qt app build config (sources, QML, Qt modules) |
| `app/src/main.cpp` | Qt app entry point |
| `coder-dlp-compositor/include/coderdlp.h` | Public C API for DLP compositor |
| `coder-vpn-linux/cmd/coder-desktop-helper/main.go` | Go helper entry point |
| `dbus/com.coder.Desktop.Helper1.xml` | D-Bus interface definition |
| `packaging/polkit/com.coder.Desktop.Helper1.policy` | Polkit policy for privileged operations |
| `packaging/systemd/coder-desktop-helper.service` | systemd service unit for helper |

### Key Directories

| Directory | Contents |
|-----------|----------|
| `app/src/api/` | REST API client (`CoderApiClient`) and WebSocket base |
| `app/src/auth/` | Authentication and login flow |
| `app/src/dlp/` | DLP compositor manager (Qt-side integration) |
| `app/src/settings/` | `SettingsManager`, `MdmConfigManager` (three-layer settings) |
| `app/src/vpn/` | VPN control logic (D-Bus calls to helper) |
| `app/src/tray/` | System tray icon and menu |
| `app/src/models/` | Qt models (workspaces, agents, etc.) |
| `app/qml/` | QML UI pages (Main, VPN, Workspaces, Settings, SecureDev, etc.) |
| `app/tests/unit/` | Qt unit tests (`tst_*.cpp`) |
| `coder-vpn-linux/internal/dbusservice/` | Go D-Bus service implementation |
| `dbus/` | D-Bus config, interface XML, `.service` files |

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

### Disable DLP compositor

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DENABLE_DLP=OFF
```

Skips wlroots/wayland dependencies and hides the Secure Dev tab in the UI.

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

```bash
cd coder-vpn-linux && gofmt -w .
```

## Lint

CI runs `clang-format --dry-run --Werror` (LLVM 21) on all C/C++ sources. There is no separate C++ linter (no clang-tidy config). For Go, use standard `go vet`:

```bash
# Check C/C++ formatting (dry run — reports errors without modifying files)
find app/src coder-dlp-compositor/src -type f \( -name '*.h' -o -name '*.cpp' -o -name '*.c' \) \
  -exec clang-format --dry-run --Werror {} +

# Go vet
cd coder-vpn-linux && go vet ./...
```

## Clean

```bash
# Remove CMake build directory
rm -rf build/

# Go clean
cd coder-vpn-linux && go clean ./...
```

## Scripts

| Script | Purpose |
|--------|---------|
| `scripts/install-hooks.sh` | Install git pre-commit hook (symlinks `scripts/pre-commit`) |
| `packaging/release.sh` | Build release packages (tar.gz, .deb, .rpm) |
| `packaging/appimage/build-appimage.sh` | Create AppImage from a Release build |

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
# Qt app unit tests (requires offscreen platform when headless / CI)
QT_QPA_PLATFORM=offscreen ctest --test-dir build/app --output-on-failure

# Go tests
cd coder-vpn-linux && go test ./...

# DLP compositor tests
ctest --test-dir build/coder-dlp-compositor --output-on-failure
```

### Testing Patterns

- Qt tests live in `app/tests/unit/` and use the **QTest** framework (`QTest`, `QSignalSpy`, `QTemporaryDir`).
- Test files are named `tst_<topic>.cpp` (e.g., `tst_settings.cpp`, `tst_models.cpp`, `tst_apiclient.cpp`).
- Tests exercise components in isolation. For example, `tst_settings.cpp` validates the three-layer settings model by writing temporary policy JSON files and asserting that `SettingsManager` resolves values correctly.
- When running Qt tests without a display server, set `QT_QPA_PLATFORM=offscreen`.
- Go tests use standard `testing` package conventions.

### Pre-commit Validation Checklist

Before committing, run at minimum:

1. **Format** — `find app/src coder-dlp-compositor/src -type f \( -name '*.h' -o -name '*.cpp' -o -name '*.c' \) -exec clang-format -i {} +`
2. **Build** — `cmake --build build -j$(nproc)`
3. **Test** — `QT_QPA_PLATFORM=offscreen ctest --test-dir build/app --output-on-failure`
4. **Go** (if touching `coder-vpn-linux/`) — `cd coder-vpn-linux && go test ./... && go vet ./...`

## Commit and Pull Request Guidelines

### Commit Messages

This project uses **conventional commits** (`type(scope): message`):

- **Types:** `feat`, `fix`, `refactor`, `test`, `docs`, `chore`, `ci`
- **Scope** (optional): component name — `auth`, `settings`, `dlp`, `qml`, `sandbox`, etc.
- **Examples:**
  - `feat(settings): add 6 missing settings UI elements`
  - `fix(auth): use QQuickWebEngineProfile for QML WebEngineView compatibility`
  - `test: add tests for sandbox config extensions, RunningAppModel, and AppLaunchProfile`
  - `fix: clang-format all C/C++ sources`

Use imperative mood in the subject line. Keep the subject under 72 characters.

### Pull Request Requirements

1. All CI checks must pass (lint + build + tests for all three components).
2. Format all touched C/C++ files with `clang-format` before pushing.
3. If adding new source files, update `app/CMakeLists.txt` (sources list) accordingly.
4. If adding new QML files, register them in the `qt_add_qml_module()` call in `app/CMakeLists.txt`.

## What NOT To Do

1. **Do NOT modify `coder/coder`** — The upstream Coder repository is a dependency, not part of this repo. Any VPN/tunnel SDK changes must be proposed upstream.

2. **Do NOT use X11 for DLP** — The DLP compositor MUST use Wayland (wlroots). X11 cannot enforce clipboard/screenshot restrictions because any client can read any other client's windows. This is a fundamental security requirement.

3. **Do NOT run the helper as an unprivileged user** — It requires `CAP_NET_ADMIN` for TUN/route management and runs as root via D-Bus activation. Attempting to run it unprivileged will fail to create TUN devices and manage routes.

4. **Do NOT run the wlroots event loop on a separate thread** — Integrate via `QSocketNotifier` on the Qt main thread. wlroots is not thread-safe.

5. **Do NOT store secrets in settings files** — Use `libsecret` for API tokens and session credentials. Settings files are plaintext JSON.

6. **Do NOT bypass the settings layer resolution** — Always go through `SettingsManager` to read settings. Never read user prefs or MDM policy directly in UI code.

7. **Do NOT add new source files without updating CMakeLists.txt** — Every new `.cpp`, `.h`, or `.qml` file in `app/` must be registered in `app/CMakeLists.txt`. The build will silently succeed but your code won't be compiled.

8. **Do NOT use bare `new`/`delete` outside of Qt parent ownership** — Use `std::unique_ptr` or `std::shared_ptr`. `new QFoo(parent)` is fine because Qt's parent-child tree manages lifetime. See C++ Coding Standards above.
