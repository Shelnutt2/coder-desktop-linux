# Coder Desktop for Linux

[![CI](https://github.com/coder/coder-desktop-linux/actions/workflows/ci.yml/badge.svg)](https://github.com/coder/coder-desktop-linux/actions/workflows/ci.yml)
[![License](https://img.shields.io/github/license/coder/coder-desktop-linux)](LICENSE)

A native Linux desktop application for managing [Coder](https://coder.com) remote development workspaces. Provides one-click VPN connectivity, a full workspace management UI, AI task monitoring, and optional Data Loss Prevention (DLP) enforcement — all from your system tray.

![Coder Desktop for Linux](docs/screenshots/main-window.png)

## Features

- **VPN Connectivity** — Seamless Tailscale/WireGuard tunnels to your Coder workspaces with DNS-based routing. Connect, disconnect, and monitor status from the system tray.
- **Workspace Management** — Browse, start, stop, and monitor workspaces across one or more Coder deployments. View build logs and workspace agents in real time.
- **AI Task Monitoring** — Track AI-powered coding tasks running inside your workspaces with live status updates.
- **Data Loss Prevention (DLP)** — Optional Wayland compositor sandbox that enforces clipboard, screenshot, and file-access policies on workspace applications. Managed via MDM or user settings.
- **Multi-Deployment Support** — Connect to multiple Coder deployments simultaneously with per-deployment credentials.
- **Three-Layer Settings** — User preferences, MDM policy overrides (`/etc/coder-desktop/policy.json`), and compiled defaults. Administrators can lock any setting via MDM.
- **Secure Credential Storage** — API tokens stored via `libsecret` (GNOME Keyring / KWallet) with encrypted-file fallback for headless environments.
- **Auto-Update Notifications** — Checks GitHub Releases for new versions on startup and notifies you when an update is available.

## Requirements

| Dependency | Version | Required | Notes |
|------------|---------|----------|-------|
| Linux | — | ✅ | x86_64 or aarch64 |
| Qt | ≥ 6.5 | ✅ | Core, Gui, Quick, QuickControls2, Network, Widgets |
| CMake | ≥ 3.21 | ✅ | Build system |
| Go | ≥ 1.25 | ✅ | For building `libcodervpn.so` |
| wlroots | ≥ 0.19 | DLP only | Wayland compositor for DLP sandbox |
| wayland | — | DLP only | Client/server libraries |
| libxkbcommon | — | DLP only | Keyboard handling in compositor |
| libsecret | — | Recommended | Credential storage (GNOME Keyring / KWallet) |
| bubblewrap | — | DLP only | Sandbox launcher for file/network isolation |
| pkg-config | — | ✅ | Dependency resolution for C libraries |

> **Wayland note:** DLP features require a Wayland session. On X11 sessions, the application runs normally but DLP enforcement is unavailable (X11 cannot enforce clipboard/screenshot restrictions due to its security model). The app gracefully degrades — all non-DLP features work everywhere.

## Quick Install

Pre-built packages are available for common distributions. See the [`packaging/`](packaging/) directory for package sources.

### Debian / Ubuntu (.deb)

```bash
sudo dpkg -i coder-desktop_0.1.0_amd64.deb
sudo apt-get install -f  # resolve dependencies
```

### Fedora / RHEL (.rpm)

```bash
sudo dnf install coder-desktop-0.1.0-1.x86_64.rpm
```

### Flatpak

```bash
flatpak install coder-desktop.flatpak
```

### AppImage

```bash
chmod +x Coder_Desktop-0.1.0-x86_64.AppImage
./Coder_Desktop-0.1.0-x86_64.AppImage
```

## Build from Source

### Install dependencies

<details>
<summary>Debian / Ubuntu</summary>

```bash
sudo apt install build-essential cmake golang-go \
    qt6-base-dev qt6-declarative-dev qt6-webengine-dev \
    libwlroots-dev libwayland-dev libxkbcommon-dev \
    libsecret-1-dev bubblewrap pkg-config
```

</details>

<details>
<summary>Fedora</summary>

```bash
sudo dnf install cmake golang qt6-qtbase-devel qt6-qtdeclarative-devel \
    qt6-qtwebengine-devel wlroots-devel wayland-devel libxkbcommon-devel \
    libsecret-devel bubblewrap pkg-config
```

</details>

### Build

```bash
# Configure
cmake -B build -DCMAKE_BUILD_TYPE=Debug

# Build all targets
cmake --build build -j$(nproc)

# Run
./build/app/coder-desktop
```

### Build individual targets

```bash
cmake --build build --target codervpn_so      # Go VPN shared library
cmake --build build --target coderdlp         # DLP compositor library
cmake --build build --target coder-desktop    # Qt desktop application
```

## Architecture

Coder Desktop is a monorepo producing three build targets that compose at runtime:

```mermaid
graph TD
    APP["coder-desktop<br/><i>Qt 6 / C++20</i>"]
    VPN["libcodervpn.so<br/><i>Go c-shared</i>"]
    DLP["libcoderdlp.so<br/><i>C / wlroots</i>"]
    API["Coder Deployment<br/><i>REST API</i>"]

    APP -->|dlopen| VPN
    APP -->|dlopen| DLP
    APP -->|HTTPS| API
    VPN -->|WireGuard tunnel| API

    style APP fill:#0969da,color:#fff
    style VPN fill:#2ea043,color:#fff
    style DLP fill:#8250df,color:#fff
    style API fill:#656d76,color:#fff
```

| Component | Language | Source | Description |
|-----------|----------|--------|-------------|
| `coder-desktop` | C++20 / QML | [`app/`](app/) | Qt 6 desktop app — UI, tray, settings, credential storage |
| `libcodervpn.so` | Go | [`coder-vpn-linux/`](coder-vpn-linux/) | Coder VPN/tunnel SDK wrapper via C ABI |
| `libcoderdlp.so` | C | [`coder-dlp-compositor/`](coder-dlp-compositor/) | Nested Wayland compositor for DLP enforcement |

### Directory Structure

```
coder-desktop-linux/
├── CMakeLists.txt              # Top-level build (orchestrates all 3 targets)
├── app/                        # Qt 6 / C++ → coder-desktop
│   ├── src/                    # C++ sources
│   ├── qml/                    # QML UI files
│   └── tests/                  # Unit tests
├── coder-vpn-linux/            # Go → libcodervpn.so
│   ├── bridge.go               # //export functions (C API)
│   └── internal/               # Go internals (tunnel, auth, DNS)
├── coder-dlp-compositor/       # C / wlroots → libcoderdlp.so
│   ├── include/coderdlp.h      # Public C API
│   └── src/                    # Compositor implementation
├── packaging/                  # deb, rpm, flatpak, AppImage configs
├── docs/                       # Architecture and design documents
└── .github/workflows/          # CI pipelines
```

## Configuration

### Settings Layers

Settings resolve through three layers (highest priority first):

1. **MDM policy** — `/etc/coder-desktop/policy.json` (read-only, can lock individual settings)
2. **User preferences** — `~/.config/coder-desktop/settings.json`
3. **Compiled defaults**

When no MDM policy file is present, all settings are user-editable through the Settings UI. Administrators can deploy a policy file to enforce and lock specific settings across managed machines.

### Credential Storage

API tokens and session credentials are stored via the [Secret Service API](https://specifications.freedesktop.org/secret-service/latest/) (`libsecret`), which integrates with GNOME Keyring, KWallet, or any compatible secret store. An encrypted-file fallback is used in headless or keyring-less environments.

> **Security:** Credentials are never stored in plaintext settings files.

### Key Settings

| Setting | Default | Description |
|---------|---------|-------------|
| `deploymentUrl` | *(empty)* | Primary Coder deployment URL |
| `autoConnectVpn` | `false` | Connect VPN automatically on startup |
| `dlpEnabled` | `false` | Enable DLP compositor sandbox |
| `dlpClipboardBlock` | `false` | Block clipboard copy/paste in DLP mode |
| `dlpScreenshotBlock` | `false` | Block screenshots in DLP mode |
| `checkForUpdates` | `true` | Check GitHub Releases for new versions |
| `notificationsEnabled` | `true` | Show desktop notifications |
| `theme` | `system` | UI theme (`system`, `light`, `dark`) |

## Data Loss Prevention (DLP)

The DLP feature runs workspace applications inside a nested Wayland compositor (`libcoderdlp.so`, built on [wlroots](https://gitlab.freedesktop.org/wlroots/wlroots)). This compositor enforces:

- **Clipboard isolation** — Blocks copy/paste between workspace apps and the host desktop
- **Screenshot prevention** — Prevents screen capture of workspace content
- **File sandbox** — Restricts file system access via [bubblewrap](https://github.com/containers/bubblewrap)
- **Network sandbox** — Limits network access to approved endpoints

### Requirements

- **Wayland session** — DLP requires a Wayland compositor (GNOME on Wayland, KDE Plasma on Wayland, Sway, etc.)
- **wlroots ≥ 0.19** — Build dependency for the compositor
- **bubblewrap** — Runtime dependency for file/network sandboxing

### X11 Graceful Degradation

On X11 sessions, DLP features are automatically disabled. The application detects the session type at startup and hides DLP-related UI elements. All other features (VPN, workspace management, settings) work normally.

> **Why not X11?** The X11 protocol allows any client to read any other client's windows and input. This makes clipboard isolation and screenshot prevention fundamentally impossible to enforce. Wayland's security model provides the per-client isolation required for DLP.

## Contributing

### Build & Run

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
./build/app/coder-desktop
```

### Run Tests

```bash
# Qt app unit tests
cd build && ctest --test-dir app --output-on-failure

# Go tests
cd coder-vpn-linux && go test ./...

# All tests
cd build && ctest --output-on-failure
```

### Code Style

- **C++20** with Qt 6.5+ — follows the [C++ Core Guidelines](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines)
- RAII for all resource management; `std::unique_ptr` / `std::shared_ptr` for ownership
- `[[nodiscard]]` on factory functions and error codes
- Qt parent-child ownership for QObject-derived types
- `enum class` for all enumerations

### Project Guidelines

See [`AGENTS.md`](AGENTS.md) for detailed architecture notes, coding standards, and integration patterns.

## License

See [LICENSE](LICENSE).
