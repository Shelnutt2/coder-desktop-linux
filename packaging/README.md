# Packaging — Coder Desktop for Linux

Two approaches are available for building distributable packages.

## Quick: CPack (cross-distro, single command)

CPack is integrated into the CMake build. It auto-detects which generators
are available on your system (tar.gz is always available; .deb requires
`dpkg-deb`; .rpm requires `rpmbuild`).

```bash
# Option A: use the convenience script
./packaging/release.sh

# Option B: manual steps
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build -j$(nproc)
cmake --build build --target package     # produces .tar.gz (+ .deb / .rpm)
```

Packages are written to `build/coder-desktop-<version>-<arch>.*`.

## Distro-Native: deb / rpm

For proper distro integration (PPA uploads, COPR, OBS), use the configs in
`deb/` and `rpm/`. These follow distro-specific conventions and produce
packages suitable for official repositories.

### Debian / Ubuntu

```bash
# From the project root (requires debhelper, devscripts, build deps):
dpkg-buildpackage -us -uc -b
```

The Debian packaging files are in `deb/debian/`:
- `control` — package metadata + dependencies
- `rules` — build recipe (uses dh + cmake)
- `changelog`, `copyright`, `compat`

### Fedora / RHEL / openSUSE

```bash
# Build the RPM (requires rpm-build + build deps):
rpmbuild -ba rpm/coder-desktop.spec \
    --define "_sourcedir $(pwd)/.." \
    --define "_topdir $(pwd)/rpmbuild"
```

The spec file is `rpm/coder-desktop.spec`.

## Install Prefix

By default, `cmake --install build` installs to `<project>/dist/` for local
development. Override with `-DCMAKE_INSTALL_PREFIX=/usr` for system install
(used by the release script and distro packaging).

### Install layout

| Path | Content |
|------|---------|
| `<prefix>/bin/coder-desktop` | Main application binary |
| `<prefix>/lib/coder-desktop/libcoderdlp.so` | DLP compositor library |
| `<prefix>/lib/coder-desktop/libcodervpn.so` | VPN shared library |
| `<prefix>/share/applications/com.coder.CoderDesktop.desktop` | Desktop entry |
| `<prefix>/share/icons/hicolor/scalable/apps/coder-desktop.svg` | App icon |

## Dependencies

All external dependencies are system-installed packages. See
`cmake/FindDependencies.cmake` for the full list and detection logic.

| Dependency | Detection Method | Required? |
|-----------|-----------------|-----------|
| Qt 6 (≥ 6.5) | `find_package(Qt6)` | Yes |
| Qt WebSockets | `find_package(Qt6 WebSockets)` | Optional |
| Qt WebEngine | `find_package(Qt6 WebEngineQuick)` | Optional |
| wlroots (0.19) | `pkg_check_modules(wlroots-0.19)` | Yes |
| Wayland | `pkg_check_modules(wayland-server)` | Yes |
| libsecret | `pkg_check_modules(libsecret-1)` | Optional |
| Go (≥ 1.21) | `find_program(go)` | For VPN library |

No vcpkg, Conan, or FetchContent — see the rationale in
`cmake/FindDependencies.cmake`.
