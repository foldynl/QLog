#!/usr/bin/env bash

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
SPEC="$ROOT/packaging/rpm/qlog.spec"
TOPDIR="/tmp/qlog-rpmbuild"
VERSION="$(sed -n 's/^VERSION *= *//p' "$ROOT/QLog.pro" | head -n 1)"

if [[ -z "$VERSION" ]]; then
    echo "Error: QLog version was not found in $ROOT/QLog.pro." >&2
    exit 1
fi

export REPO_VERSION="$VERSION"

FEDORA="$(rpm -E '%fedora')"
ARCH="$(rpm -E '%{_arch}')"
DIST="$ROOT/dist/rpm/fedora-${FEDORA}/${ARCH}"

rm -rf "$TOPDIR"
mkdir -p \
    "$TOPDIR/BUILD" \
    "$TOPDIR/BUILDROOT" \
    "$TOPDIR/RPMS" \
    "$TOPDIR/SOURCES" \
    "$TOPDIR/SPECS" \
    "$TOPDIR/SRPMS" \
    "$DIST"

tar \
    --exclude-vcs \
    --exclude-vcs-ignores \
    --exclude='./.agents' \
    --exclude='./.codex' \
    --exclude='./build' \
    --exclude='./build-appimage' \
    --exclude='./dist' \
    --exclude='./tests/build' \
    --transform "s,^\\.,QLog-${VERSION}," \
    -czf "$TOPDIR/SOURCES/qlog-${VERSION}.tar.gz" \
    -C "$ROOT" .

cp "$SPEC" "$TOPDIR/SPECS/qlog.spec"

rpmbuild \
    --define "_topdir $TOPDIR" \
    -ba "$TOPDIR/SPECS/qlog.spec"

find "$TOPDIR/RPMS" -type f -name '*.rpm' -exec cp -v {} "$DIST/" \;
find "$TOPDIR/SRPMS" -type f -name '*.rpm' -exec cp -v {} "$DIST/" \;

if ! find "$DIST" -maxdepth 1 -type f -name '*.rpm' -print -quit | grep -q .; then
    echo "Error: no RPM package was created." >&2
    exit 1
fi

echo
echo "Created:"
find "$DIST" -maxdepth 1 -type f -name '*.rpm' -print
