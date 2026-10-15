#!/usr/bin/env bash

set -euo pipefail

PACKAGE_TYPE="$1"
shift
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
CONTAINER_ENGINE="${CONTAINER_ENGINE:-podman}"

usage()
{
    echo "Usage: packaging/$PACKAGE_TYPE/build-container.sh [VERSION] [--smoke-test]" >&2
    echo "Ubuntu: YY.MM (default 24.04); Fedora: 43 or later (default 43)." >&2
    exit 1
}

if [[ $# -gt 2 || ( $# -eq 2 && "$2" != --smoke-test ) ]]; then
    usage
fi

RUN_OPTIONS=(--rm --security-opt label=disable)
case "$PACKAGE_TYPE" in
    deb)
        VERSION="${1:-24.04}"
        [[ "$VERSION" =~ ^[0-9]{2}\.[0-9]{2}$ ]] || usage
        BASE_IMAGE="docker.io/library/ubuntu:$VERSION"
        RUN_OPTIONS+=(-e UBUNTU_VERSION="$VERSION" -e DEBIAN_FRONTEND=noninteractive)
        ;;
    rpm)
        VERSION="${1:-43}"
        [[ "$VERSION" =~ ^[1-9][0-9]*$ && "$VERSION" -ge 43 ]] || usage
        BASE_IMAGE="registry.fedoraproject.org/fedora:$VERSION"
        ;;
    *)
        usage
        ;;
esac

ARCH="$(uname -m)"
case "$ARCH" in
    x86_64|amd64|aarch64|arm64) ;;
    *) echo "Error: unsupported host architecture: $ARCH" >&2; exit 1 ;;
esac

case "${CONTAINER_ENGINE##*/}" in
    podman) RUN_OPTIONS+=(--userns=keep-id) ;;
    docker) ;;
    *) echo "Error: supported container engines are podman and docker." >&2; exit 1 ;;
esac

MODE=build
SOURCE_MOUNT="$ROOT:/src"
if [[ $# -eq 2 ]]; then
    MODE=smoke-test
    SOURCE_MOUNT+=:ro
fi

echo "Running QLog $PACKAGE_TYPE $MODE for $BASE_IMAGE on $ARCH with ${CONTAINER_ENGINE##*/}"

exec "$CONTAINER_ENGINE" run \
    "${RUN_OPTIONS[@]}" \
    --user 0:0 \
    -e HOME=/tmp \
    -e HOST_UID="$(id -u)" \
    -e HOST_GID="$(id -g)" \
    -e JOBS="${JOBS:-}" \
    -v "$SOURCE_MOUNT" \
    -w /src \
    "$BASE_IMAGE" \
    "./packaging/$PACKAGE_TYPE/build-in-container.sh" "$MODE"
