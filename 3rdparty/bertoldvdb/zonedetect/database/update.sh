#!/usr/bin/env bash

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../../../.." && pwd)"
DATABASE_DIR="$ROOT/3rdparty/bertoldvdb/zonedetect/database"
VERSIONS="$DATABASE_DIR/versions.sh"
DATABASE="$ROOT/res/data/timezone21.bin"
OK_MARK="✓"
OUTDATED_MARK="✗"
if [[ -t 1 ]]
then
    OK_MARK=$'\033[32m✓\033[0m'
    OUTDATED_MARK=$'\033[31m✗\033[0m'
fi

# shellcheck source=versions.sh
source "$VERSIONS"

MODE="${1:---check}"
TARGET="${2:-all}"
case "$MODE:$TARGET" in
    --check:all|--check:timezone-boundary|--check:natural-earth|\
    --update:all|--update:timezone-boundary|--update:natural-earth)
        ;;
    *)
        echo "Usage: $0 [--check|--update] [all|timezone-boundary|natural-earth]" >&2
        exit 1
        ;;
esac

TEMP_DIR="$(mktemp -d)"
trap 'rm -rf "$TEMP_DIR"' EXIT
UPDATE_AVAILABLE=0
LATEST_TIMEZONE_VERSION="$TIMEZONE_BOUNDARY_VERSION"
LATEST_TIMEZONE_SHA256="$TIMEZONE_BOUNDARY_SHA256"
LATEST_NATURAL_EARTH_SHA256="$NATURAL_EARTH_SHA256"
TIMEZONE_ARCHIVE=""
NATURAL_EARTH_ARCHIVE=""

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

latest_release()
{
    local url tag
    url="$(curl -fsSL --retry 3 -o /dev/null -w '%{url_effective}' \
        "$TIMEZONE_BOUNDARY_REPOSITORY/releases/latest")"
    url="${url%/}"
    tag="${url##*/}"
    [[ -n "$tag" && "$tag" != "latest" ]] || die "cannot determine the latest timezone release"
    printf '%s\n' "$tag"
}

require_clean()
{
    git -C "$ROOT" diff --quiet -- "${VERSIONS#"$ROOT"/}" "${DATABASE#"$ROOT"/}" ||
        die "timezone dependency files have local changes"
    git -C "$ROOT" diff --cached --quiet -- "${VERSIONS#"$ROOT"/}" "${DATABASE#"$ROOT"/}" ||
        die "timezone dependency files have staged changes"
}

check_timezone_boundary()
{
    LATEST_TIMEZONE_VERSION="$(latest_release)"
    if [[ "$LATEST_TIMEZONE_VERSION" == "$TIMEZONE_BOUNDARY_VERSION" ]]; then
        echo "timezone-boundary-builder: $TIMEZONE_BOUNDARY_VERSION is up to date ${OK_MARK}"
        return
    fi

    UPDATE_AVAILABLE=1
    echo "timezone-boundary-builder: update available ($TIMEZONE_BOUNDARY_VERSION -> $LATEST_TIMEZONE_VERSION) ${OUTDATED_MARK}"
    [[ "$MODE" == "--update" ]] || return 0

    TIMEZONE_ARCHIVE="$TEMP_DIR/timezones-with-oceans.shapefile.zip"
    download \
        "$TIMEZONE_BOUNDARY_REPOSITORY/releases/download/$LATEST_TIMEZONE_VERSION/timezones-with-oceans.shapefile.zip" \
        "$TIMEZONE_ARCHIVE"
    LATEST_TIMEZONE_SHA256="$(sha256sum "$TIMEZONE_ARCHIVE" | awk '{print $1}')"
}

check_natural_earth()
{
    local actual
    NATURAL_EARTH_ARCHIVE="$TEMP_DIR/ne_10m_admin_0_countries_lakes.zip"
    download "$NATURAL_EARTH_URL" "$NATURAL_EARTH_ARCHIVE"
    actual="$(sha256sum "$NATURAL_EARTH_ARCHIVE" | awk '{print $1}')"

    if [[ "$actual" == "$NATURAL_EARTH_SHA256" ]]; then
        echo "Natural Earth countries: source archive is up to date ${OK_MARK}"
        return
    fi

    UPDATE_AVAILABLE=1
    LATEST_NATURAL_EARTH_SHA256="$actual"
    echo "Natural Earth countries: source archive changed ${OUTDATED_MARK}"
}

if [[ "$MODE" == "--update" ]]; then
    require_clean
fi

if [[ "$TARGET" == "all" || "$TARGET" == "timezone-boundary" ]]; then
    check_timezone_boundary
fi
if [[ "$TARGET" == "all" || "$TARGET" == "natural-earth" ]]; then
    check_natural_earth
fi

if [[ "$MODE" == "--check" ]]; then
    [[ "$UPDATE_AVAILABLE" -eq 0 ]] || exit 2
    exit 0
fi

[[ "$UPDATE_AVAILABLE" -ne 0 ]] || exit 0

WORK_DIR="$TEMP_DIR/build" \
TIMEZONE_ARCHIVE="$TIMEZONE_ARCHIVE" \
NATURAL_EARTH_ARCHIVE="$NATURAL_EARTH_ARCHIVE" \
NATURAL_EARTH_SHA256_OVERRIDE="$LATEST_NATURAL_EARTH_SHA256" \
    "$DATABASE_DIR/builder/makedb.sh" \
        "$LATEST_TIMEZONE_VERSION" "$LATEST_TIMEZONE_SHA256"

cp "$TEMP_DIR/build/out_v1/timezone21.bin" "$DATABASE"
cat > "$VERSIONS.tmp" <<EOF
# This file is maintained by 3rdparty/bertoldvdb/zonedetect/database/update.sh.

TIMEZONE_BOUNDARY_REPOSITORY="${TIMEZONE_BOUNDARY_REPOSITORY}"
TIMEZONE_BOUNDARY_VERSION="${LATEST_TIMEZONE_VERSION}"
TIMEZONE_BOUNDARY_SHA256="${LATEST_TIMEZONE_SHA256}"

NATURAL_EARTH_URL="${NATURAL_EARTH_URL}"
NATURAL_EARTH_SHA256="${LATEST_NATURAL_EARTH_SHA256}"
EOF
mv "$VERSIONS.tmp" "$VERSIONS"
