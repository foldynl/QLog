#!/usr/bin/env bash

set -euo pipefail

: "${HOST_UID:?}"
: "${HOST_GID:?}"

if [[ "${1:-build}" == smoke-test ]]; then
    VERSION="$(sed -n 's/^VERSION *= *//p' QLog.pro | head -n 1)"
    if [[ -z "$VERSION" ]]; then
        echo "Error: QLog version was not found in QLog.pro." >&2
        exit 1
    fi

    ARCH="$(rpm -E '%{_arch}')"
    DIST="/src/dist/rpm/fedora-$(rpm -E '%fedora')/${ARCH}"
    shopt -s nullglob
    PACKAGES=("$DIST"/QLog-"$VERSION"-*."$ARCH".rpm)
    if [[ ${#PACKAGES[@]} -ne 1 ]]; then
        echo "Error: expected one QLog $VERSION RPM package in $DIST." >&2
        exit 1
    fi

    dnf install -y util-linux "${PACKAGES[@]}"
    if ! HELP_OUTPUT="$(setpriv --reuid="$HOST_UID" --regid="$HOST_GID" --clear-groups \
        env QT_QPA_PLATFORM=offscreen qlog --help 2>&1)"; then
        printf '%s\n' "$HELP_OUTPUT" >&2
        exit 1
    fi
    printf '%s\n' "$HELP_OUTPUT"
    grep -Fq "QLog Help" <<< "$HELP_OUTPUT"
    exit
fi

dnf -y install \
    rpm-build \
    rpmdevtools \
    dnf-plugins-core \
    git \
    tar \
    gzip \
    util-linux
REPO_VERSION=0 dnf builddep -y packaging/rpm/qlog.spec
dnf clean all

exec setpriv --reuid="$HOST_UID" --regid="$HOST_GID" --clear-groups \
    ./packaging/rpm/build.sh
