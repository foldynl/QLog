#!/usr/bin/env bash

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
GPG_KEY_ID="${1:-}"

if [[ $# -gt 1 ]]; then
    echo "Usage: $0 [GPG_KEY_ID]" >&2
    exit 1
fi

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

    # Match the source exclusions in packaging/rpm/build.sh. Personal Git
    # excludes must not hide files that the RPM source archive would include.
    untracked_files="$(git -C "$ROOT" ls-files --others --exclude-per-directory=.gitignore \
        --exclude=/.agents --exclude=/.codex --exclude=/build \
        --exclude=/build-appimage --exclude=/dist --exclude=/tests/build)"
    if [[ -n "$untracked_files" ]]; then
        echo "Error: untracked files would be included in the RPM sources:" >&2
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

BUILD_STARTED="$(mktemp)"
trap 'rm -f "$BUILD_STARTED"' EXIT

for fedora in 43 44; do
    "$ROOT/packaging/rpm/build-container.sh" "$fedora"
done

"$ROOT/packaging/appimage/build-container.sh"

if [[ -n "$GPG_KEY_ID" ]]; then
    verify_release_sources
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
