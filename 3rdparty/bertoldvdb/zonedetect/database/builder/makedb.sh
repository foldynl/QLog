#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
DATABASE_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

# shellcheck source=../versions.sh
source "$DATABASE_DIR/versions.sh"

if [[ $# -gt 2 ]]; then
    echo "Usage: $0 [TIMEZONE_VERSION [TIMEZONE_SHA256]]" >&2
    exit 1
fi

TIMEZONE_VERSION="${1:-$TIMEZONE_BOUNDARY_VERSION}"
TIMEZONE_SHA256="${2:-$TIMEZONE_BOUNDARY_SHA256}"
WORK_DIR="${WORK_DIR:-$SCRIPT_DIR}"
TIMEZONE_ARCHIVE="${TIMEZONE_ARCHIVE:-}"
NATURAL_EARTH_ARCHIVE="${NATURAL_EARTH_ARCHIVE:-}"
NATURAL_EARTH_SHA256="${NATURAL_EARTH_SHA256_OVERRIDE:-$NATURAL_EARTH_SHA256}"

download()
{
    curl -fsSL --retry 3 -o "$2" "$1"
}

mkdir -p "$WORK_DIR"
rm -rf "$WORK_DIR/out" "$WORK_DIR/out_v1" \
       "$WORK_DIR/naturalearth" "$WORK_DIR/timezone" "$WORK_DIR/db.zip"
mkdir -p "$WORK_DIR/out" "$WORK_DIR/out_v1" \
         "$WORK_DIR/naturalearth" "$WORK_DIR/timezone"

g++ "$SCRIPT_DIR/builder.cpp" --std=c++11 -o "$WORK_DIR/builder" -lshp

if [[ -n "$NATURAL_EARTH_ARCHIVE" ]]; then
    cp "$NATURAL_EARTH_ARCHIVE" "$WORK_DIR/naturalearth/ne.zip"
else
    download "$NATURAL_EARTH_URL" "$WORK_DIR/naturalearth/ne.zip"
fi
if [[ -n "$TIMEZONE_ARCHIVE" ]]; then
    cp "$TIMEZONE_ARCHIVE" "$WORK_DIR/timezone/tz.zip"
else
    download \
        "$TIMEZONE_BOUNDARY_REPOSITORY/releases/download/$TIMEZONE_VERSION/timezones-with-oceans.shapefile.zip" \
        "$WORK_DIR/timezone/tz.zip"
fi

echo "$NATURAL_EARTH_SHA256  $WORK_DIR/naturalearth/ne.zip" | sha256sum --check --status
echo "$TIMEZONE_SHA256  $WORK_DIR/timezone/tz.zip" | sha256sum --check --status

cd "$WORK_DIR/naturalearth"
unzip ne.zip
cd "$WORK_DIR"

(
echo C "naturalearth/ne_10m_admin_0_countries_lakes ./out/country16.bin 16 \"Made with Natural Earth, placed in the Public Domain.\" 0"
echo C "naturalearth/ne_10m_admin_0_countries_lakes ./out/country21.bin 21 \"Made with Natural Earth, placed in the Public Domain.\" 0"
echo C "naturalearth/ne_10m_admin_0_countries_lakes ./out_v1/country16.bin 16 \"Made with Natural Earth, placed in the Public Domain.\" 1"
echo C "naturalearth/ne_10m_admin_0_countries_lakes ./out_v1/country21.bin 21 \"Made with Natural Earth, placed in the Public Domain.\" 1"
) | xargs -n6 -P4 ./builder

cd "$WORK_DIR/timezone"
unzip tz.zip
cd "$WORK_DIR"

(
echo "T timezone/combined-shapefile-with-oceans ./out/timezone16.bin 16 \"Contains data from Natural Earth, placed in the Public Domain. Contains information from https://github.com/evansiroky/timezone-boundary-builder, which is made available here under the Open Database License \(ODbL\).\" 0"
echo "T timezone/combined-shapefile-with-oceans ./out/timezone21.bin 21 \"Contains data from Natural Earth, placed in the Public Domain. Contains information from https://github.com/evansiroky/timezone-boundary-builder, which is made available here under the Open Database License \(ODbL\).\" 0"
echo "T timezone/combined-shapefile-with-oceans ./out_v1/timezone16.bin 16 \"Contains data from Natural Earth, placed in the Public Domain. Contains information from https://github.com/evansiroky/timezone-boundary-builder, which is made available here under the Open Database License \(ODbL\).\" 1"
echo "T timezone/combined-shapefile-with-oceans ./out_v1/timezone21.bin 21 \"Contains data from Natural Earth, placed in the Public Domain. Contains information from https://github.com/evansiroky/timezone-boundary-builder, which is made available here under the Open Database License \(ODbL\).\" 1"
) | xargs -n6 -P4 ./builder

rm -rf "$WORK_DIR/timezone" "$WORK_DIR/naturalearth"

cd "$WORK_DIR"
zip db.zip out/* out_v1/*
