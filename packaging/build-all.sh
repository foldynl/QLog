#!/usr/bin/env bash

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
GPG_KEY_ID="${1:-}"

if [[ $# -gt 1 ]]; then
    echo "Usage: $0 [GPG_KEY_ID]" >&2
    exit 1
fi

if [[ -n "$GPG_KEY_ID" ]]; then
    if ! command -v gpg >/dev/null 2>&1; then
        echo "Error: gpg was not found." >&2
        exit 1
    fi

    if ! gpg --batch --list-secret-keys "$GPG_KEY_ID" >/dev/null 2>&1; then
        echo "Error: GPG secret key was not found: $GPG_KEY_ID" >&2
        exit 1
    fi
fi

BUILD_STARTED="$(mktemp)"
trap 'rm -f "$BUILD_STARTED"' EXIT

for fedora in 43 44; do
    "$ROOT/packaging/rpm/build-container.sh" "$fedora"
done

"$ROOT/packaging/appimage/build-container.sh"

if [[ -n "$GPG_KEY_ID" ]]; then
    echo
    echo "Signing artifacts with GPG key $GPG_KEY_ID:"
fi

processed=0
while IFS= read -r -d '' artifact; do
    if [[ -n "$GPG_KEY_ID" ]]; then
        gpg --batch --yes --local-user "$GPG_KEY_ID" --armor --detach-sign \
            --output "$artifact.asc" "$artifact"
        echo "$artifact.asc"
    else
        rm -f -- "$artifact.asc"
    fi
    processed=1
done < <(find "$ROOT/dist/rpm" "$ROOT/dist/appimage" -type f \
    \( -name '*.rpm' -o -name '*.AppImage' \) -newer "$BUILD_STARTED" -print0)

if [[ $processed -eq 0 ]]; then
    echo "Error: no artifacts were created." >&2
    exit 1
fi
