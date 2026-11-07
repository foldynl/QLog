#!/usr/bin/env bash

set -euo pipefail

: "${HOST_UID:?}"
: "${HOST_GID:?}"

if [[ $# -ne 1 ]]; then
    echo "Usage: $0 5|6 (inside the unit-test container)" >&2
    exit 1
fi

case "$1" in
    5)
        QMAKE_BIN=qmake
        QT_PACKAGES=(qtbase5-dev qtchooser qt5-qmake qt5keychain-dev \
            libqt5serialport5-dev libqt5websockets5-dev libqt5sql5-sqlite)
        ;;
    6)
        QMAKE_BIN=qmake6
        QT_PACKAGES=(qt6-base-dev qtkeychain-qt6-dev libqt6sql6-sqlite)
        source /etc/os-release
        # Ubuntu 22.04 predates the qt6-*-dev package names.
        if [[ "$VERSION_ID" == 22.04 ]]; then
            QT_PACKAGES+=(libqt6serialport6-dev libqt6websockets6-dev)
        else
            QT_PACKAGES+=(qt6-serialport-dev qt6-websockets-dev)
        fi
        ;;
    *)
        echo "Error: supported Qt versions are 5 and 6." >&2
        exit 1
        ;;
esac

run_phase()
{
    local code="$1"
    local phase="$2"
    local status
    shift 2

    printf '\nPhase: %s\n' "$phase"
    if "$@"; then
        return 0
    else
        status=$?
    fi

    printf 'Phase failed: %s (exit %s).\n' "$phase" "$status" >&2
    exit "$code"
}

export -f run_phase

run_phase 20 "updating package lists" apt-get update
run_phase 21 "installing dependencies" apt-get install -y --no-install-recommends \
    build-essential pkg-config libhamlib-dev libsqlite3-dev zlib1g-dev libssl-dev \
    util-linux "${QT_PACKAGES[@]}"

export QT_SELECT="qt$1"
export QT_QPA_PLATFORM=offscreen
export QLOG_RUN_ADI_IMPORT_BENCHMARK="${QLOG_RUN_ADI_IMPORT_BENCHMARK:-0}"

exec setpriv --reuid="$HOST_UID" --regid="$HOST_GID" --clear-groups \
    bash -euo pipefail -c '
        printf "\nPhase: preparing sources and build environment\n"
        BUILD_DIR="$(mktemp -d /tmp/qlog-unit-tests.XXXXXX)" || exit 22
        mkdir "$BUILD_DIR/source" "$BUILD_DIR/build" || exit 22

        # Quoted moc includes can select stale files beside the original sources.
        tar \
            --exclude-vcs \
            --exclude-vcs-ignores \
            --exclude="./.agents" \
            --exclude="./.codex" \
            --exclude="./build" \
            --exclude="./build-appimage" \
            --exclude="./dist" \
            --exclude="./tests/build" \
            -C /src -cf - . | tar -C "$BUILD_DIR/source" -xf - || exit 22

        cd "$BUILD_DIR/build" || exit 22
        run_phase 23 "qmake configuration" "$1" "$BUILD_DIR/source/tests/tests.pro"
        run_phase 24 "compilation" make -j"${JOBS:-$(nproc)}"
        run_phase 25 "unit tests" make check
    ' bash "$QMAKE_BIN"
