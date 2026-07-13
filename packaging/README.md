# Packaging — Coder Desktop for Linux

Release artifacts (`.tar.gz`, `.deb`, `.rpm`, `.apk`, `.AppImage`, `.snap`)
are built automatically by `.github/workflows/release.yml` when a `v*` tag
is pushed. The sections below describe how each package is built and how to
build them locally.

## Quick: CPack (cross-distro, single command)

CPack is integrated into the CMake build. It auto-detects which generators
are available on your system (tar.gz is always available; .deb requires
`dpkg-deb`; .rpm requires `rpmbuild`). Pass
`-DCPACK_FORCE_ALL_GENERATORS=ON` (used by release CI) to enable all three
generators unconditionally so missing tools fail loudly.

```bash
# Option A: use the convenience script
./packaging/release.sh

# Option B: manual steps
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build -j$(nproc)
cmake --build build --target package     # produces .tar.gz (+ .deb / .rpm)
```

Packages are written to the build directory. The .deb and .rpm use
distro-native file names (`coder-desktop_<version>_amd64.deb`,
`coder-desktop-<version>-1.x86_64.rpm`); the tarball is
`coder-desktop-<version>-<arch>.tar.gz`.

The package version defaults to the CMake project version; release CI
overrides it with `-DCODER_DESKTOP_VERSION=<tag>` (leading `v` stripped).

The .deb and .rpm run shared post-install/post-uninstall scriptlets from
`scripts/` that reload systemd and the D-Bus system bus configuration.

## AppImage

Requires [linuxdeploy](https://github.com/linuxdeploy/linuxdeploy) and
[linuxdeploy-plugin-qt](https://github.com/linuxdeploy/linuxdeploy-plugin-qt)
on `PATH`, and `-DCMAKE_INSTALL_PREFIX=/usr`:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build -j$(nproc)
cmake --build build --target appimage    # stages AppDir + runs linuxdeploy
```

The `appimage` target stages a `DESTDIR` install into `<build>/AppDir` and
invokes `appimage/build-appimage.sh`. The resulting `.AppImage` is written
to the build directory.

## Alpine apk

Built natively against musl with `abuild` inside an Alpine container:

```bash
docker run --rm -v "$PWD:/src" -w /src alpine:3.23 \
    sh packaging/apk/build-apk.sh 0.1.0 /src/dist
```

The APKBUILD is in `apk/APKBUILD`. The script templates the version,
creates the source tarball from the checkout, generates an ephemeral
signing key, and copies the built `.apk` files to the output directory.

## Snap

The snapcraft config is at `snap/snapcraft.yaml` (repo root). It builds the
full project inside a core24 (Ubuntu 24.04) environment with classic
confinement:

```bash
snapcraft            # requires snapcraft + LXD
sudo snap install ./coder-desktop_*.snap --classic --dangerous
```

Note: a snap cannot install the VPN helper's D-Bus system service, polkit
policy, or systemd unit onto the host. Use the .deb/.rpm/.apk for full
system integration.

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
