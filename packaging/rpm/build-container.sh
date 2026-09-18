#!/usr/bin/env bash

set -euo pipefail

CONTAINER_ENGINE="${CONTAINER_ENGINE:-podman}"
FEDORA="${1:-43}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"

if [[ $# -gt 1 || ! "$FEDORA" =~ ^[0-9]+$ ]]; then
    echo "Usage: $0 [FEDORA_VERSION]" >&2
    echo "The RPM is always built for the host architecture." >&2
    exit 1
fi

case "$(uname -m)" in
    x86_64|amd64)
        ARCH="x86_64"
        ;;
    aarch64|arm64)
        ARCH="aarch64"
        ;;
    *)
        echo "Error: unsupported host architecture: $(uname -m)" >&2
        exit 1
        ;;
esac

if ! command -v "$CONTAINER_ENGINE" >/dev/null 2>&1; then
    echo "Error: container engine not found: $CONTAINER_ENGINE" >&2
    exit 1
fi

RUN_OPTIONS=(--rm --security-opt label=disable)

case "${CONTAINER_ENGINE##*/}" in
    podman)
        RUN_OPTIONS+=(--userns=keep-id)
        ;;
    docker)
        ;;
    *)
        echo "Error: supported container engines are podman and docker." >&2
        exit 1
        ;;
esac

BASE_IMAGE="registry.fedoraproject.org/fedora:${FEDORA}"

echo "Building QLog RPM for Fedora $FEDORA natively on $ARCH with ${CONTAINER_ENGINE##*/}"

"$CONTAINER_ENGINE" run \
    "${RUN_OPTIONS[@]}" \
    --user 0:0 \
    -e HOME=/tmp \
    -e HOST_UID="$(id -u)" \
    -e HOST_GID="$(id -g)" \
    -v "$ROOT:/src" \
    -w /src \
    "$BASE_IMAGE" \
    ./packaging/rpm/build-in-container.sh
