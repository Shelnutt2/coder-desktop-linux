#!/usr/bin/env bash
# Build release packages: tar.gz + .deb (if dpkg available) + .rpm (if rpmbuild available)
#
# Usage:
#   ./packaging/release.sh              # uses build-release/
#   ./packaging/release.sh my-build     # uses my-build/
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="${1:-build-release}"

echo "=== Configuring (Release, prefix=/usr) ==="
cmake -B "$BUILD_DIR" -S "$PROJECT_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr

echo ""
echo "=== Building ==="
cmake --build "$BUILD_DIR" -j"$(nproc)"

echo ""
echo "=== Running tests ==="
ctest --test-dir "$BUILD_DIR/app" --output-on-failure || true
ctest --test-dir "$BUILD_DIR/coder-dlp-compositor" --output-on-failure || true

echo ""
echo "=== Packaging (CPack) ==="
cmake --build "$BUILD_DIR" --target package

echo ""
echo "=== Packages ==="
ls -lh "$BUILD_DIR"/coder-desktop-* 2>/dev/null || echo "(no packages found)"
