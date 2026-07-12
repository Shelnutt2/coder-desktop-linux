#!/bin/sh
# build-apk.sh — Build the Alpine .apk package natively with abuild.
#
# Intended to run as root inside an Alpine container (see
# .github/workflows/release.yml). Locally:
#
#   docker run --rm -v "$PWD:/src" -w /src alpine:3.23 \
#       sh packaging/apk/build-apk.sh 0.1.0 /src/dist
#
# Arguments:
#   $1  package version (default: 0.1.0)
#   $2  output directory for the built .apk files (default: <src>/dist)
#
# The APKBUILD source tarball is created from the current checkout, so the
# package always matches the checked-out revision. abuild refuses to run as
# root, so the build happens under a dedicated "builder" user with an
# ephemeral signing key (abuild-keygen -an).
set -eu

VERSION="${1:-0.1.0}"
SRC="$(cd "$(dirname "$0")/../.." && pwd)"
OUTDIR="${2:-$SRC/dist}"

apk add --no-cache alpine-sdk

if ! id builder >/dev/null 2>&1; then
    adduser -D builder
    addgroup builder abuild
fi

BUILDROOT=/home/builder/coder-desktop
mkdir -p "$BUILDROOT"
cp "$SRC/packaging/apk/APKBUILD" "$BUILDROOT/"
sed -i "s/^pkgver=.*/pkgver=$VERSION/" "$BUILDROOT/APKBUILD"

# Package the checkout as the source tarball abuild expects. Copy into a
# correctly named directory first; busybox tar has no --transform.
STAGING="$(mktemp -d)/coder-desktop-$VERSION"
mkdir -p "$STAGING"
cp -a "$SRC/." "$STAGING/"
rm -rf "$STAGING/.git" "$STAGING/build" "$STAGING/dist"
tar -C "$(dirname "$STAGING")" -czf "$BUILDROOT/coder-desktop-$VERSION.tar.gz" \
    "coder-desktop-$VERSION"
rm -rf "$(dirname "$STAGING")"

chown -R builder:builder "$BUILDROOT"

su builder -c "
    set -eu
    cd '$BUILDROOT'
    abuild-keygen -an
    abuild checksum
    abuild -r
"

mkdir -p "$OUTDIR"
find /home/builder/packages -name '*.apk' -exec cp {} "$OUTDIR/" \;
echo '=== Built .apk packages ==='
ls -lh "$OUTDIR"/*.apk
