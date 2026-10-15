#!/usr/bin/env bash

set -euo pipefail

: "${UBUNTU_VERSION:?}"

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
ARCH="$(dpkg --print-architecture)"
DIST="$ROOT/dist/deb/ubuntu-${UBUNTU_VERSION}/${ARCH}"
BUILD_DIR="$(mktemp -d /tmp/qlog-deb-build.XXXXXX)"
trap 'rm -rf "$BUILD_DIR"' EXIT

mkdir -p "$BUILD_DIR/source"
tar \
    --exclude-vcs \
    --exclude-vcs-ignores \
    --exclude='./.agents' \
    --exclude='./.codex' \
    --exclude='./build' \
    --exclude='./build-appimage' \
    --exclude='./dist' \
    --exclude='./tests/build' \
    -C "$ROOT" -cf - . | tar -C "$BUILD_DIR/source" -xf -

cd "$BUILD_DIR/source"
export QT_SELECT=qt5
cme check dpkg-copyright
dpkg-buildpackage -b -us -uc -j"${JOBS:-$(nproc)}"
lintian --fail-on error "$BUILD_DIR"/qlog_*.changes

shopt -s nullglob
PACKAGES=("$BUILD_DIR"/qlog_*.deb)
if [[ ${#PACKAGES[@]} -eq 0 ]]; then
    echo "Error: no DEB package was created." >&2
    exit 1
fi

mkdir -p "$DIST"
rm -f -- "$DIST"/qlog_*.deb "$DIST"/qlog_*.changes "$DIST"/qlog_*.buildinfo
cp -v "${PACKAGES[@]}" "$BUILD_DIR"/qlog_*.changes "$BUILD_DIR"/qlog_*.buildinfo "$DIST/"
