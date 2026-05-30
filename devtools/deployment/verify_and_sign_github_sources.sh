#!/usr/bin/env bash

# Download GitHub-generated source archives for a tag via gh CLI, verify
# their content against the local repository tag, then GPG-sign both archives.
#
# Usage:
#   ./devtools/deployment/verify_and_sign_github_sources.sh <tag> [gpg_key_id]
#
# Example:
#   ./devtools/deployment/verify_and_sign_github_sources.sh v0.50.0
#   ./devtools/deployment/verify_and_sign_github_sources.sh v0.50.0 0xDEADBEEF

# ########## CONFIG
ARTIFACT_DIR="/home/foldynl/QLog_releases/release_artifacts"
# ########### END OF CONFIG

set -euo pipefail

if [[ $# -lt 1 ]]; then
    echo "Usage: $0 <tag> [gpg_key_id]"
    exit 1
fi

TAG="$1"
GPG_KEY_ID="${2:-}"

if ! command -v gh > /dev/null 2>&1; then
    echo "ERROR: gh CLI is required."
    exit 1
fi

if ! command -v gpg > /dev/null 2>&1; then
    echo "ERROR: gpg is required."
    exit 1
fi

if ! command -v unzip > /dev/null 2>&1; then
    echo "ERROR: unzip is required."
    exit 1
fi

if ! git rev-parse --is-inside-work-tree > /dev/null 2>&1; then
    echo "ERROR: must be run from inside a git repository."
    exit 1
fi

if ! git rev-parse -q --verify "refs/tags/${TAG}" > /dev/null 2>&1; then
    echo "ERROR: local tag '${TAG}' does not exist."
    exit 1
fi

if ! gh auth status > /dev/null 2>&1; then
    echo "ERROR: gh is not authenticated. Run 'gh auth login' first."
    exit 1
fi

REPO_FULL="$(gh repo view --json nameWithOwner -q .nameWithOwner)"
REPO_NAME="$(gh repo view --json name -q .name)"

if ! gh release view "${TAG}" > /dev/null 2>&1; then
    echo "ERROR: GitHub release '${TAG}' does not exist."
    exit 1
fi

mkdir -p "${ARTIFACT_DIR}"

TAR_FILE="${ARTIFACT_DIR}/${TAG}.tar.gz"
ZIP_FILE="${ARTIFACT_DIR}/${TAG}.zip"

TMPDIR="$(mktemp -d)"
trap 'rm -rf "${TMPDIR}"' EXIT

echo "Downloading GitHub source archives for ${REPO_FULL} ${TAG}..."
gh api "/repos/${REPO_FULL}/tarball/${TAG}" > "${TAR_FILE}"
gh api "/repos/${REPO_FULL}/zipball/${TAG}" > "${ZIP_FILE}"

verify_archive() {
    local archive="$1"
    local extract_dir="$2"
    local fmt="$3"

    mkdir -p "${extract_dir}"

    if [[ "${fmt}" == "tar" ]]; then
        tar -xzf "${archive}" -C "${extract_dir}"
    elif [[ "${fmt}" == "zip" ]]; then
        unzip -q "${archive}" -d "${extract_dir}"
    else
        echo "ERROR: unknown archive format '${fmt}'."
        exit 1
    fi

    local root_dir
    root_dir="$(find "${extract_dir}" -mindepth 1 -maxdepth 1 -type d | head -n 1)"
    if [[ -z "${root_dir}" ]]; then
        echo "ERROR: could not find extracted top-level directory in ${archive}."
        exit 1
    fi

    local git_files
    local arc_files
    git_files="${TMPDIR}/${fmt}_git_files.txt"
    arc_files="${TMPDIR}/${fmt}_arc_files.txt"

    git ls-tree -r --name-only "${TAG}" | sort > "${git_files}"
    find "${root_dir}" -type f | sed "s#^${root_dir}/##" | sort > "${arc_files}"

    if ! diff -u "${git_files}" "${arc_files}" > "${TMPDIR}/${fmt}_files.diff"; then
        echo "ERROR: file list mismatch for ${archive}."
        cat "${TMPDIR}/${fmt}_files.diff"
        exit 1
    fi

    echo "File list OK for ${archive}. Checking file content hashes..."

    local mismatches=0
    local rel_path
    while IFS= read -r rel_path; do
        local hash_git
        local hash_arc
        hash_git="$(git show "${TAG}:${rel_path}" | sha256sum | awk '{print $1}')"
        hash_arc="$(sha256sum "${root_dir}/${rel_path}" | awk '{print $1}')"
        if [[ "${hash_git}" != "${hash_arc}" ]]; then
            echo "MISMATCH: ${rel_path}"
            mismatches=$((mismatches + 1))
        fi
    done < "${git_files}"

    if [[ "${mismatches}" -ne 0 ]]; then
        echo "ERROR: ${mismatches} file(s) differ in ${archive}."
        exit 1
    fi

    echo "Content hashes OK for ${archive}."
}

verify_archive "${TAR_FILE}" "${TMPDIR}/tar_extract" "tar"
verify_archive "${ZIP_FILE}" "${TMPDIR}/zip_extract" "zip"

echo "All checks passed. Creating GPG signatures..."

if [[ -n "${GPG_KEY_ID}" ]]; then
    gpg --armor --detach-sign --local-user "${GPG_KEY_ID}" "${TAR_FILE}"
    gpg --armor --detach-sign --local-user "${GPG_KEY_ID}" "${ZIP_FILE}"
else
    gpg --armor --detach-sign "${TAR_FILE}"
    gpg --armor --detach-sign "${ZIP_FILE}"
fi

echo "Uploading signatures to release ${TAG}..."
gh release upload "${TAG}" "${TAR_FILE}.asc" "${ZIP_FILE}.asc" --clobber

echo ""
echo "Done."
echo "Signed files:"
echo "  ${TAR_FILE}.asc"
echo "  ${ZIP_FILE}.asc"
echo "Uploaded to release ${TAG}:"
echo "  $(basename "${TAR_FILE}.asc")"
echo "  $(basename "${ZIP_FILE}.asc")"
