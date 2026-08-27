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

IMAGE="localhost/qlog-rpm-build:f${FEDORA}-${ARCH}-$$"

echo "Building QLog RPM for Fedora $FEDORA natively on $ARCH with ${CONTAINER_ENGINE##*/}"

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
    --build-arg FEDORA_VERSION="$FEDORA" \
    -f "$ROOT/packaging/rpm/Containerfile" \
    -t "$IMAGE" \
    "$ROOT/packaging/rpm"

"$CONTAINER_ENGINE" run \
    "${RUN_OPTIONS[@]}" \
    -e HOME=/tmp \
    -v "$ROOT:/src" \
    -w /src \
    "$IMAGE" \
    ./packaging/rpm/build.sh
