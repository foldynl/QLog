#!/usr/bin/env bash

set -euo pipefail

: "${HOST_UID:?}"
: "${HOST_GID:?}"

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
