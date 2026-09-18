#!/usr/bin/env bash

set -euo pipefail

: "${HOST_UID:?}"
: "${HOST_GID:?}"

apt-get update
{
    echo util-linux
    sed -e '/^[[:space:]]*#/d' -e '/^[[:space:]]*$/d' \
        packaging/appimage/packages.txt \
        packaging/appimage/runtime/bundle-packages.txt
} | sort -u | xargs -r apt-get install -y
rm -rf /var/lib/apt/lists/*

exec setpriv --reuid="$HOST_UID" --regid="$HOST_GID" --clear-groups \
    ./packaging/appimage/build.sh
