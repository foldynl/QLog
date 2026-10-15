#!/usr/bin/env bash

set -euo pipefail

: "${HOST_UID:?}"
: "${HOST_GID:?}"
: "${UBUNTU_VERSION:?}"

apt-get update

if [[ "${1:-build}" == smoke-test ]]; then
    DIST="/src/dist/deb/ubuntu-${UBUNTU_VERSION}/$(dpkg --print-architecture)"
    shopt -s nullglob
    PACKAGES=("$DIST"/qlog_*.deb)
    if [[ ${#PACKAGES[@]} -ne 1 ]]; then
        echo "Error: expected one QLog DEB package in $DIST." >&2
        exit 1
    fi

    apt-get install -y util-linux "${PACKAGES[@]}"
    if ! HELP_OUTPUT="$(setpriv --reuid="$HOST_UID" --regid="$HOST_GID" --clear-groups \
        env QT_QPA_PLATFORM=offscreen qlog --help 2>&1)"; then
        printf '%s\n' "$HELP_OUTPUT" >&2
        exit 1
    fi
    printf '%s\n' "$HELP_OUTPUT"
    grep -Fq "QLog Help" <<< "$HELP_OUTPUT"
    exit
fi

apt-get install -y \
    build-essential devscripts equivs dpkg-dev fakeroot lintian \
    libconfig-model-dpkg-perl licensecheck git util-linux

# Keep dependency-package build files out of the mounted checkout.
cd "$(mktemp -d /tmp/qlog-deb-deps.XXXXXX)"
mk-build-deps --install --remove /src/debian/control \
    --tool "apt-get -y --no-install-recommends"
cd /src
apt-get clean

exec setpriv --reuid="$HOST_UID" --regid="$HOST_GID" --clear-groups \
    ./packaging/deb/build.sh
