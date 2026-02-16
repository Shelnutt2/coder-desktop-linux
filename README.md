# Coder Desktop for Linux

A native Linux desktop application for [Coder](https://coder.com) that provides VPN connectivity to workspaces, a full management UI, and optional Data Loss Prevention (DLP) enforcement via a Wayland compositor.

## Architecture

The project is a monorepo with three build targets:

| Target | Description | Source |
|--------|-------------|--------|
| `libcodervpn.so` | Go c-shared library wrapping Coder's VPN/tunnel SDK | `coder-vpn-linux/` |
| `libcoderdlp.so` | wlroots-based nested Wayland compositor for DLP | `coder-dlp-compositor/` |
| `coder-desktop` | Qt 6 / C++ desktop application | `app/` |

The Qt application loads both shared libraries at runtime. The VPN library provides workspace connectivity via Tailscale/WireGuard tunnels. The DLP compositor optionally wraps workspace applications in a sandboxed Wayland session that enforces clipboard, screenshot, and file-access policies.

## Prerequisites

| Dependency | Version | Notes |
|------------|---------|-------|
| CMake | ≥ 3.21 | Build system |
| Go | ≥ 1.25 | For building `libcodervpn.so` |
| Qt | ≥ 6.5 | Widgets, Quick, Network, WebEngine |
| wlroots | ≥ 0.19 | DLP compositor |
| wayland | — | Wayland client/server libraries |
| libxkbcommon | — | Keyboard handling |
| libsecret | — | Credential storage (GNOME Keyring / KWallet) |
| bubblewrap (`bwrap`) | — | Sandbox launcher for DLP file/network isolation |
| pkg-config | — | Dependency resolution for C libraries |

### Install on Debian/Ubuntu

```bash
sudo apt install build-essential cmake golang-go \
    qt6-base-dev qt6-declarative-dev qt6-webengine-dev \
    libwlroots-dev libwayland-dev libxkbcommon-dev \
    libsecret-1-dev bubblewrap pkg-config
```

### Install on Fedora

```bash
sudo dnf install cmake golang qt6-qtbase-devel qt6-qtdeclarative-devel \
    qt6-qtwebengine-devel wlroots-devel wayland-devel libxkbcommon-devel \
    libsecret-devel bubblewrap pkg-config
```

## Build

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
cmake --build build --target codervpn_so      # Go VPN library
cmake --build build --target coderdlp         # DLP compositor
cmake --build build --target coder-desktop    # Qt application
```

## Directory Structure

```
coder-desktop-linux/
├── CMakeLists.txt              # Top-level build (orchestrates all 3 targets)
├── coder-vpn-linux/            # Go → libcodervpn.so
├── coder-dlp-compositor/       # C / wlroots → libcoderdlp.so
├── app/                        # Qt 6 / C++ → coder-desktop
├── packaging/                  # deb, rpm, flatpak, AppImage configs
├── docs/                       # Architecture and design documents
└── .github/workflows/          # CI pipelines
```

## Testing

```bash
# All tests
cd build && ctest --output-on-failure

# Go tests only
cd coder-vpn-linux && go test ./...
```

## License

See [LICENSE](LICENSE).
