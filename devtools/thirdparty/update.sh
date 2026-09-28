#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
LOCK_FILE="${ROOT_DIR}/3rdparty/versions.sh"

# shellcheck source=../../3rdparty/versions.sh
source "${LOCK_FILE}"

MODE="${1:---check}"
TARGET="${2:-all}"
if [[ "${MODE}" != "--check" && "${MODE}" != "--update" ]] ||
   [[ "${TARGET}" != "all" && "${TARGET}" != "csv-parser" && "${TARGET}" != "zonedetect" ]]
then
    echo "Usage: $0 [--check|--update] [all|csv-parser|zonedetect]" >&2
    exit 1
fi

TEMP_DIR="$(mktemp -d)"
trap 'rm -rf "${TEMP_DIR}"' EXIT
UPDATE_AVAILABLE=0

die()
{
    echo "Error: $*" >&2
    exit 1
}

download()
{
    curl -fsSL --retry 3 -o "$2" "$1"
    [[ -s "$2" ]] || die "downloaded file is empty: $1"
}

require_clean()
{
    git -C "${ROOT_DIR}" diff --quiet -- "$@" || die "vendored files have local changes"
    git -C "${ROOT_DIR}" diff --cached --quiet -- "$@" || die "vendored files have staged changes"
}

save_versions()
{
    cat > "${LOCK_FILE}.tmp" <<EOF
# This file is maintained by devtools/thirdparty/update.sh.

CSV_PARSER_REPOSITORY="${CSV_PARSER_REPOSITORY}"
CSV_PARSER_TAG="${CSV_PARSER_TAG}"

ZONEDETECT_REPOSITORY="${ZONEDETECT_REPOSITORY}"
ZONEDETECT_COMMIT="${ZONEDETECT_COMMIT}"
EOF
    mv "${LOCK_FILE}.tmp" "${LOCK_FILE}"
}

check_csv_parser()
{
    local directory="${ROOT_DIR}/3rdparty/vincentlaucsb/csv-parser"
    local repository_path="${CSV_PARSER_REPOSITORY#https://github.com/}"
    local release_url
    release_url="$(curl -fsSL --retry 3 -o /dev/null -w '%{url_effective}' \
                        "${CSV_PARSER_REPOSITORY}/releases/latest")"

    local tag="${release_url##*/}"
    local version="${tag#v}"
    [[ -n "${tag}" && "${tag}" != "latest" ]] || die "cannot determine csv-parser release"

    local header="${TEMP_DIR}/csv.hpp"
    local license="${TEMP_DIR}/csv-parser.LICENSE"
    download "${CSV_PARSER_REPOSITORY}/releases/download/${tag}/csv.hpp" "${header}"
    download "https://raw.githubusercontent.com/${repository_path}/${tag}/LICENSE" "${license}"
    grep -Fq "CSV for C++, version ${version}" "${header}" || die "invalid csv-parser header"

    if [[ "${tag}" == "${CSV_PARSER_TAG}" ]] &&
       cmp -s "${header}" "${directory}/csv.hpp" &&
       cmp -s "${license}" "${directory}/LICENSE"
    then
        echo "csv-parser: ${CSV_PARSER_TAG#v} is up to date"
        return
    fi

    UPDATE_AVAILABLE=1
    echo "csv-parser: update available (${CSV_PARSER_TAG#v} -> ${version})"
    [[ "${MODE}" == "--update" ]] || return 0

    require_clean "${directory#${ROOT_DIR}/}/csv.hpp" "${directory#${ROOT_DIR}/}/LICENSE"
    cp "${header}" "${directory}/csv.hpp"
    cp "${license}" "${directory}/LICENSE"
    CSV_PARSER_TAG="${tag}"
    save_versions
}

check_zonedetect()
{
    local directory="${ROOT_DIR}/3rdparty/bertoldvdb/zonedetect"
    local repository_path="${ZONEDETECT_REPOSITORY#https://github.com/}"
    local commit
    commit="$(git ls-remote "${ZONEDETECT_REPOSITORY}.git" refs/heads/master | awk '{print $1}')"
    [[ "${commit}" =~ ^[0-9a-f]{40}$ ]] || die "cannot determine ZoneDetect commit"

    local source="${TEMP_DIR}/zonedetect.c"
    local header="${TEMP_DIR}/zonedetect.h"
    local license="${TEMP_DIR}/zonedetect.LICENSE"
    local raw_url="https://raw.githubusercontent.com/${repository_path}/${commit}"
    download "${raw_url}/library/zonedetect.c" "${source}"
    download "${raw_url}/library/zonedetect.h" "${header}"
    download "${raw_url}/LICENSE" "${license}"
    grep -Fq 'ZoneDetect *ZDOpenDatabase' "${header}" || die "invalid ZoneDetect header"

    if [[ "${commit}" == "${ZONEDETECT_COMMIT}" ]] &&
       cmp -s "${source}" "${directory}/library/zonedetect.c" &&
       cmp -s "${header}" "${directory}/library/zonedetect.h" &&
       cmp -s "${license}" "${directory}/LICENSE"
    then
        echo "ZoneDetect: ${ZONEDETECT_COMMIT:0:12} is up to date"
        return
    fi

    UPDATE_AVAILABLE=1
    if [[ "${commit}" == "${ZONEDETECT_COMMIT}" ]]
    then
        echo "ZoneDetect: vendored files differ from commit ${ZONEDETECT_COMMIT:0:12}"
    else
        echo "ZoneDetect: update available (${ZONEDETECT_COMMIT:0:12} -> ${commit:0:12})"
    fi
    [[ "${MODE}" == "--update" ]] || return 0

    require_clean "${directory#${ROOT_DIR}/}/library/zonedetect.c" \
                  "${directory#${ROOT_DIR}/}/library/zonedetect.h" \
                  "${directory#${ROOT_DIR}/}/LICENSE"
    cp "${source}" "${directory}/library/zonedetect.c"
    cp "${header}" "${directory}/library/zonedetect.h"
    cp "${license}" "${directory}/LICENSE"
    ZONEDETECT_COMMIT="${commit}"
    save_versions
}

if [[ "${TARGET}" == "all" || "${TARGET}" == "csv-parser" ]]
then
    check_csv_parser
fi

if [[ "${TARGET}" == "all" || "${TARGET}" == "zonedetect" ]]
then
    check_zonedetect
fi

if [[ "${MODE}" == "--check" && "${UPDATE_AVAILABLE}" -ne 0 ]]
then
    exit 2
fi
