#!/usr/bin/env bash

set -euo pipefail

USAGE="Usage: $0 [UBUNTU_VERSION] [qt5|qt6]"

if [[ $# -eq 1 && "$1" == --help ]]; then
    echo "$USAGE"
    exit 0
fi

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
source "$ROOT/packaging/build-all.conf"
QT_VERSIONS=(5 6)

if [[ $# -gt 0 && "$1" =~ ^[0-9]{2}\.[0-9]{2}$ ]]; then
    if [[ " ${UBUNTU_VERSIONS[*]} " != *" $1 "* ]]; then
        echo "Error: Ubuntu version is not configured in packaging/build-all.conf: $1." >&2
        exit 1
    fi
    UBUNTU_VERSIONS=("$1")
    shift
fi

if [[ $# -gt 1 ]]; then
    echo "$USAGE" >&2
    exit 1
fi

if [[ $# -eq 1 ]]; then
    case "$1" in
        qt5|qt6) QT_VERSIONS=("${1#qt}") ;;
        *) echo "$USAGE" >&2; exit 1 ;;
    esac
fi

CONTAINER_ENGINE="${CONTAINER_ENGINE:-podman}"
SUCCESS_COLOR=""
FAILURE_COLOR=""
RESET_COLOR=""

if [[ -t 1 && "${TERM:-dumb}" != dumb ]]; then
    SUCCESS_COLOR=$'\033[32m'
    FAILURE_COLOR=$'\033[33m'
    RESET_COLOR=$'\033[0m'
fi

trap 'printf "\n%sRESULT: Unit tests interrupted.%s\n" "$FAILURE_COLOR" "$RESET_COLOR"; exit 130' INT
trap 'printf "\n%sRESULT: Unit tests interrupted.%s\n" "$FAILURE_COLOR" "$RESET_COLOR"; exit 143' TERM

if [[ "${#UBUNTU_VERSIONS[@]}" -eq 0 ]]; then
    echo "RESULT: No Ubuntu versions configured; unit tests skipped."
    exit 0
fi

if [[ -n "${JOBS:-}" && ! "$JOBS" =~ ^[1-9][0-9]*$ ]]; then
    printf '%sRESULT: Unit tests could not start: JOBS must be a positive integer.%s\n' \
        "$FAILURE_COLOR" "$RESET_COLOR" >&2
    exit 1
fi

RUN_OPTIONS=(--rm --security-opt label=disable)
case "${CONTAINER_ENGINE##*/}" in
    podman) RUN_OPTIONS+=(--userns=keep-id) ;;
    docker) ;;
    *)
        echo "Error: supported container engines are podman and docker." >&2
        exit 1
        ;;
esac

if ! command -v "$CONTAINER_ENGINE" >/dev/null 2>&1; then
    printf '%sRESULT: Unit tests could not start: container engine not found: %s.%s\n' \
        "$FAILURE_COLOR" "$CONTAINER_ENGINE" "$RESET_COLOR" >&2
    exit 1
fi

RESULTS=()
PASSED=0

for ubuntu in "${UBUNTU_VERSIONS[@]}"; do
    for qt in "${QT_VERSIONS[@]}"; do
        printf '\nRunning unit tests: Ubuntu %s / Qt%s\n' "$ubuntu" "$qt"

        if "$CONTAINER_ENGINE" run \
            "${RUN_OPTIONS[@]}" \
            --user 0:0 \
            -e DEBIAN_FRONTEND=noninteractive \
            -e LANG=C.UTF-8 \
            -e LC_ALL=C.UTF-8 \
            -e HOME=/tmp \
            -e HOST_UID="$(id -u)" \
            -e HOST_GID="$(id -g)" \
            -e JOBS="${JOBS:-}" \
            -e QLOG_RUN_ADI_IMPORT_BENCHMARK="${QLOG_RUN_ADI_IMPORT_BENCHMARK:-0}" \
            -v "$ROOT:/src:ro" \
            -w /src \
            "docker.io/library/ubuntu:$ubuntu" \
            ./tests/run-in-container.sh "$qt"; then
            RESULTS+=("PASS  Ubuntu $ubuntu / Qt$qt")
            PASSED=$((PASSED + 1))
        else
            EXIT_STATUS=$?
            # Phase failures are assigned these exit codes by the container helper.
            case "$EXIT_STATUS" in
                20) FAILURE_PHASE="updating package lists" ;;
                21) FAILURE_PHASE="installing dependencies" ;;
                22) FAILURE_PHASE="preparing sources and build environment" ;;
                23) FAILURE_PHASE="qmake configuration" ;;
                24) FAILURE_PHASE="compilation" ;;
                25) FAILURE_PHASE="unit tests" ;;
                *) FAILURE_PHASE="container startup or execution (exit $EXIT_STATUS)" ;;
            esac
            RESULTS+=("FAIL  Ubuntu $ubuntu / Qt$qt")
            break 2
        fi
    done
done

echo
printf '%s\n' "${RESULTS[@]}"
echo

if [[ "$PASSED" -eq "${#RESULTS[@]}" ]]; then
    printf '%sRESULT: All unit tests passed (%s/%s).%s\n' \
        "$SUCCESS_COLOR" "$PASSED" "${#RESULTS[@]}" "$RESET_COLOR"
else
    printf '%sRESULT: Unit tests failed (%s/%s passed; stopped at first failure).%s\n' \
        "$FAILURE_COLOR" "$PASSED" "${#RESULTS[@]}" "$RESET_COLOR"
    printf '%sFAILED: Ubuntu %s / Qt%s; phase: %s.%s\n' \
        "$FAILURE_COLOR" "$ubuntu" "$qt" "$FAILURE_PHASE" "$RESET_COLOR"
    exit 1
fi
