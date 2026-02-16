#!/usr/bin/env bash
# build-appimage.sh — Create an AppImage for Coder Desktop.
#
# Prerequisites:
#   - The project is already built with cmake (Release mode).
#   - linuxdeploy and linuxdeploy-plugin-qt are on PATH (or set
#     LINUXDEPLOY / LINUXDEPLOY_PLUGIN_QT environment variables).
#
# Usage:
#   cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
#   cmake --build build -j$(nproc)
#   DESTDIR=AppDir cmake --install build
#   ./packaging/appimage/build-appimage.sh [AppDir]

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"

APPDIR="${1:-${PROJECT_ROOT}/AppDir}"

LINUXDEPLOY="${LINUXDEPLOY:-linuxdeploy}"
LINUXDEPLOY_PLUGIN_QT="${LINUXDEPLOY_PLUGIN_QT:-linuxdeploy-plugin-qt}"

# Ensure the AppDir has our private libraries on the library path so
# linuxdeploy picks them up for bundling.
export LD_LIBRARY_PATH="${APPDIR}/usr/lib/coder-desktop${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

# Tell the Qt plugin where to find qmake
export QMAKE="${QMAKE:-qmake6}"

echo "==> Building AppImage from ${APPDIR}"

"${LINUXDEPLOY}" \
    --appdir "${APPDIR}" \
    --desktop-file "${APPDIR}/usr/share/applications/com.coder.CoderDesktop.desktop" \
    --icon-file "${APPDIR}/usr/share/icons/hicolor/scalable/apps/coder-desktop.svg" \
    --plugin qt \
    --output appimage

echo "==> Done. AppImage created in current directory."
