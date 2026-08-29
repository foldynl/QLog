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

BUILD_OPTIONS=(--rm --force-rm)
RUN_OPTIONS=(--rm --security-opt label=disable)

case "${CONTAINER_ENGINE##*/}" in
    podman)
        BUILD_OPTIONS+=(--layers=false)
        RUN_OPTIONS+=(--userns=keep-id)
        ;;
    docker)
        RUN_OPTIONS+=(--user "$(id -u):$(id -g)")
        ;;
    *)
        echo "Error: supported container engines are podman and docker." >&2
        exit 1
        ;;
esac

IMAGE="localhost/qlog-appimage-build:${ARCH}-$$"

echo "Building QLog AppImage natively for $ARCH with ${CONTAINER_ENGINE##*/}"

cleanup()
{
    local status=$?
    local cleanup_status=0

    trap - EXIT

    if "$CONTAINER_ENGINE" image inspect "$IMAGE" >/dev/null 2>&1; then
        echo "Removing temporary build image $IMAGE"
        "$CONTAINER_ENGINE" image rm --force "$IMAGE" >/dev/null || cleanup_status=$?
    fi

    if [[ $status -eq 0 && $cleanup_status -ne 0 ]]; then
        status=$cleanup_status
    fi

    exit "$status"
}

trap cleanup EXIT

"$CONTAINER_ENGINE" build \
    "${BUILD_OPTIONS[@]}" \
    -f "$ROOT/packaging/appimage/Containerfile" \
    -t "$IMAGE" \
    "$ROOT/packaging/appimage"

"$CONTAINER_ENGINE" run \
    "${RUN_OPTIONS[@]}" \
    -e HOME=/tmp \
    -e APPIMAGE_EXTRACT_AND_RUN=1 \
    -e JOBS="${JOBS:-}" \
    -v "$ROOT:/src" \
    -w /src \
    "$IMAGE" \
    ./packaging/appimage/build.sh
