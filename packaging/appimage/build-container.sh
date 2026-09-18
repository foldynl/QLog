#!/usr/bin/env bash

set -euo pipefail

CONTAINER_ENGINE="${CONTAINER_ENGINE:-podman}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"

if [[ $# -ne 0 ]]; then
    echo "Usage: $0" >&2
    echo "The AppImage is always built for the host architecture." >&2
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

BASE_IMAGE="docker.io/library/debian:12"

echo "Building QLog AppImage natively for $ARCH with ${CONTAINER_ENGINE##*/}"

"$CONTAINER_ENGINE" run \
    "${RUN_OPTIONS[@]}" \
    --user 0:0 \
    -e DEBIAN_FRONTEND=noninteractive \
    -e HOME=/tmp \
    -e HOST_UID="$(id -u)" \
    -e HOST_GID="$(id -g)" \
    -e APPIMAGE_EXTRACT_AND_RUN=1 \
    -e JOBS="${JOBS:-}" \
    -v "$ROOT:/src" \
    -w /src \
    "$BASE_IMAGE" \
    ./packaging/appimage/build-in-container.sh
