#!/usr/bin/env bash

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
GPG_KEY_ID="${1:-}"

if [[ $# -gt 1 ]]; then
    echo "Usage: $0 [GPG_KEY_ID]" >&2
    exit 1
fi

source "$ROOT/packaging/build-all.conf"

verify_release_sources()
{
    local current_commit
    local tracked_changes
    local untracked_files

    current_commit="$(git -C "$ROOT" rev-parse --verify "refs/tags/${RELEASE_TAG}^{commit}")"
    if [[ "$current_commit" != "$RELEASE_COMMIT" ]]; then
        echo "Error: release tag $RELEASE_TAG changed during the build." >&2
        exit 1
    fi

    current_commit="$(git -C "$ROOT" rev-parse HEAD)"
    if [[ "$current_commit" != "$RELEASE_COMMIT" ]]; then
        echo "Error: HEAD must point to release tag $RELEASE_TAG ($RELEASE_COMMIT)." >&2
        exit 1
    fi

    tracked_changes="$(git -C "$ROOT" status --porcelain=v1 --untracked-files=no)"
    if [[ -n "$tracked_changes" ]]; then
        echo "Error: tracked files have uncommitted changes:" >&2
        printf '%s\n' "$tracked_changes" >&2
        exit 1
    fi

    # Match the DEB and RPM source exclusions. Personal Git excludes must not
    # hide files that the package sources would include.
    untracked_files="$(git -C "$ROOT" ls-files --others --exclude-per-directory=.gitignore \
        --exclude=/.agents --exclude=/.codex --exclude=/build \
        --exclude=/build-appimage --exclude=/dist --exclude=/tests/build)"
    if [[ -n "$untracked_files" ]]; then
        echo "Error: untracked files would be included in the package sources:" >&2
        printf '%s\n' "$untracked_files" >&2
        exit 1
    fi
}

if [[ -n "$GPG_KEY_ID" ]]; then
    if ! command -v gpg >/dev/null 2>&1; then
        echo "Error: gpg was not found." >&2
        exit 1
    fi

    if ! gpg --batch --list-secret-keys "$GPG_KEY_ID" >/dev/null 2>&1; then
        echo "Error: GPG secret key was not found: $GPG_KEY_ID" >&2
        exit 1
    fi

    VERSION="$(sed -n 's/^VERSION *= *//p' "$ROOT/QLog.pro" | head -n 1)"
    if [[ -z "$VERSION" ]]; then
        echo "Error: QLog version was not found in $ROOT/QLog.pro." >&2
        exit 1
    fi

    RELEASE_TAG="v${VERSION}"
    if ! RELEASE_COMMIT="$(git -C "$ROOT" rev-parse --verify "refs/tags/${RELEASE_TAG}^{commit}" 2>/dev/null)"; then
        echo "Error: local release tag $RELEASE_TAG does not exist or does not point to a commit." >&2
        exit 1
    fi

    verify_release_sources
    echo "Release sources verified: $RELEASE_TAG ($RELEASE_COMMIT)"
fi

BUILD_DIR="$(mktemp -d)"
trap 'rm -rf "$BUILD_DIR"' EXIT
touch "$BUILD_DIR/started"

mkdir -p "$ROOT/dist/deb" "$ROOT/dist/rpm" "$ROOT/dist/appimage"

for ubuntu in "${UBUNTU_VERSIONS[@]}"; do
    "$ROOT/packaging/deb/build-container.sh" "$ubuntu"
    "$ROOT/packaging/deb/build-container.sh" "$ubuntu" --smoke-test
done

for fedora in "${FEDORA_VERSIONS[@]}"; do
    "$ROOT/packaging/rpm/build-container.sh" "$fedora"
    "$ROOT/packaging/rpm/build-container.sh" "$fedora" --smoke-test
done

"$ROOT/packaging/appimage/build-container.sh"

if [[ -n "$GPG_KEY_ID" ]]; then
    verify_release_sources
    echo
    echo "Signing artifacts with GPG key $GPG_KEY_ID:"
fi

# Collect artifacts synchronously so a failed scan stops the script.
find "$ROOT/dist/deb" "$ROOT/dist/rpm" "$ROOT/dist/appimage" -type f \
    \( -name '*.deb' -o -name '*.rpm' -o -name '*.AppImage' \) -newer "$BUILD_DIR/started" -print0 > "$BUILD_DIR/artifacts"

if [[ ! -s "$BUILD_DIR/artifacts" ]]; then
    echo "Error: no artifacts were created." >&2
    exit 1
fi

while IFS= read -r -d '' artifact; do
    if [[ -n "$GPG_KEY_ID" ]]; then
        gpg --batch --yes --local-user "$GPG_KEY_ID" --armor --detach-sign \
            --output "$artifact.asc" "$artifact"
        echo "$artifact.asc"
    else
        rm -f -- "$artifact.asc"
    fi
done < "$BUILD_DIR/artifacts"

echo
echo "============================================================"
echo "SUCCESS: All configured builds and checks passed."
echo "Artifacts: $ROOT/dist"
echo "============================================================"
