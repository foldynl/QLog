#!/usr/bin/env bash

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
PACKAGING="$ROOT/packaging/appimage"
VERSIONS="$PACKAGING/versions.sh"
CHECKSUMS="$PACKAGING/checksums.sha256"
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
    --check:all|--check:hamlib|--check:qtkeychain|--check:linuxdeploy|--check:linuxdeploy-plugin-qt|\
    --update:hamlib|--update:qtkeychain|--update:linuxdeploy|--update:linuxdeploy-plugin-qt)
        ;;
    *)
        echo "Usage: $0 --check [all|hamlib|qtkeychain|linuxdeploy|linuxdeploy-plugin-qt]" >&2
        echo "       $0 --update [hamlib|qtkeychain|linuxdeploy|linuxdeploy-plugin-qt]" >&2
        exit 1
        ;;
esac

TEMP_DIR="$(mktemp -d)"
trap 'rm -rf "$TEMP_DIR"' EXIT
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

latest_release()
{
    local url tag
    url="$(curl -fsSL --retry 3 -o /dev/null -w '%{url_effective}' "$1/releases/latest")"
    tag="${url%/}"
    tag="${tag##*/}"
    [[ -n "$tag" && "$tag" != "latest" ]] || die "cannot determine the latest release of $1"
    printf '%s\n' "$tag"
}

replace_checksum()
{
    local old_name="$1" new_name="$2" value="$3"
    grep -Fq "  $old_name" "$CHECKSUMS" || die "checksum entry not found: $old_name"
    awk -v old="$old_name" -v new="$new_name" -v value="$value" '
        $2 == old { print value "  " new; next }
        { print }
    ' "$CHECKSUMS" > "$CHECKSUMS.tmp"
    mv "$CHECKSUMS.tmp" "$CHECKSUMS"
}

set_version()
{
    sed "s/^$1=.*/$1=\"$2\"/" "$VERSIONS" > "$VERSIONS.tmp"
    mv "$VERSIONS.tmp" "$VERSIONS"
}

check_release()
{
    local name="$1" repository="$2" current="$3" prefix="$4" source="$5" variable="$6"
    local latest archive url file value
    latest="$(latest_release "$repository")"

    if [[ "$latest" == "$current" ]]; then
        echo "$name: $current is up to date ${OK_MARK}"
        return
    fi

    UPDATE_AVAILABLE=1
    echo "$name: update available ($current -> $latest) ${OUTDATED_MARK}"
    [[ "$MODE" == "--update" ]] || return 0

    archive="$prefix-$latest.tar.gz"
    if [[ "$source" == "release" ]]; then
        url="$repository/releases/download/$latest/$archive"
    else
        url="$repository/archive/refs/tags/$latest.tar.gz"
    fi
    file="$TEMP_DIR/$archive"
    download "$url" "$file"
    value="$(sha256sum "$file" | awk '{print $1}')"
    replace_checksum "$prefix-$current.tar.gz" "$archive" "$value"
    set_version "$variable" "$latest"
}

check_continuous()
{
    local name="$1" repository="$2" prefix="$3" changed=0 arch file value expected
    local -a files values

    for arch in aarch64 x86_64; do
        file="$prefix-$arch.AppImage"
        download "$repository/releases/download/continuous/$file" "$TEMP_DIR/$file"
        value="$(sha256sum "$TEMP_DIR/$file" | awk '{print $1}')"
        expected="$(awk -v name="$file" '$2 == name { print $1; exit }' "$CHECKSUMS")"
        files+=("$file")
        values+=("$value")
        [[ "$value" == "$expected" ]] || changed=1
    done

    if [[ "$changed" -eq 0 ]]; then
        echo "$name: continuous assets are up to date ${OK_MARK}"
        return
    fi

    UPDATE_AVAILABLE=1
    echo "$name: updated continuous assets are available ${OUTDATED_MARK}"
    [[ "$MODE" == "--update" ]] || return 0

    for arch in 0 1; do
        replace_checksum "${files[$arch]}" "${files[$arch]}" "${values[$arch]}"
    done
}

if [[ "$MODE" == "--update" ]]; then
    git -C "$ROOT" diff --quiet -- "${VERSIONS#"$ROOT"/}" "${CHECKSUMS#"$ROOT"/}" ||
        die "AppImage dependency files have local changes"
    git -C "$ROOT" diff --cached --quiet -- "${VERSIONS#"$ROOT"/}" "${CHECKSUMS#"$ROOT"/}" ||
        die "AppImage dependency files have staged changes"
fi

[[ "$TARGET" == "all" || "$TARGET" == "hamlib" ]] &&
    check_release "Hamlib" "$HAMLIB_REPOSITORY" "$HAMLIB_VERSION" hamlib release HAMLIB_VERSION
[[ "$TARGET" == "all" || "$TARGET" == "qtkeychain" ]] &&
    check_release "QtKeychain" "$QTKEYCHAIN_REPOSITORY" "$QTKEYCHAIN_VERSION" qtkeychain tag QTKEYCHAIN_VERSION
[[ "$TARGET" == "all" || "$TARGET" == "linuxdeploy" ]] &&
    check_continuous "linuxdeploy" "$LINUXDEPLOY_REPOSITORY" linuxdeploy
[[ "$TARGET" == "all" || "$TARGET" == "linuxdeploy-plugin-qt" ]] &&
    check_continuous "linuxdeploy-plugin-qt" "$LINUXDEPLOY_QT_REPOSITORY" linuxdeploy-plugin-qt

if [[ "$MODE" == "--check" && "$UPDATE_AVAILABLE" -ne 0 ]]; then
    exit 2
fi
