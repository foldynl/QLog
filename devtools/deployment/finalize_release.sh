#!/usr/bin/env bash

# The script finalizes the release and edits the changlog in deployment scripts.
#
# Usage:
#   ./devtools/deployment/finalize_release.sh
#
# Must be executed from the QLog root directory.
# Changelog is the single source of truth.

set -uo pipefail

ROOTDIR=.
CHANGELOG="${ROOTDIR}/Changelog"
QLOG_PRO="${ROOTDIR}/QLog.pro"

if [ ! -f "${QLOG_PRO}" ]; then
    echo "ERROR: QLog.pro not found. Run this script from the QLog root directory."
    exit 1
fi

# --- Checks ---

DEB_CHANGELOG="${ROOTDIR}/debian/changelog"
RPM_SPEC="${ROOTDIR}/packaging/rpm/qlog.spec"
METAINFO="${ROOTDIR}/res/io.github.foldynl.QLog.metainfo.xml"
INSTALLER_PKG="${ROOTDIR}/installer/packages/de.dl2ic.qlog/meta/package.xml"
INSTALLER_CFG="${ROOTDIR}/installer/config/config.xml"

# Cleanup temp files on exit
TMPFILE=$(mktemp)
trap 'rm -f "${TMPFILE}"' EXIT

if ! head -n 1 "${CHANGELOG}" | grep -q '^TBC - '; then
    echo "ERROR: Changelog does not start with 'TBC'. Already finalized?"
    exit 1
fi

# --- Extract data ---

QLOG_VERSION=$(head -n 1 "${CHANGELOG}" | awk '{print $3}')
RELEASE_DATE_ISO=$(date "+%Y-%m-%d")
RELEASE_DATE_CHANGELOG=$(date "+%Y/%m/%d")
RELEASE_DATE_DEB=$(date "+%a, %-d %b %Y %T %z")
RELEASE_DATE_RPM=$(date "+%a %b %-d %Y")

# Changelog entries: lines from 2nd line until first empty line
ENTRIES=$(sed -n '2,/^$/{ /^$/d; p }' "${CHANGELOG}")

echo "Preparing release ${QLOG_VERSION} (${RELEASE_DATE_ISO})"

# --- Helper ---

xml_escape() {
    sed 's/&/\&amp;/g; s/</\&lt;/g; s/>/\&gt;/g'
}

# --- QLog.pro ---

echo "  QLog.pro"
sed -i "s/VERSION = .*/VERSION = ${QLOG_VERSION}/" "${QLOG_PRO}"

# --- Changelog ---

echo "  Changelog"
sed -i "s#^TBC - #${RELEASE_DATE_CHANGELOG} - #" "${CHANGELOG}"

# --- DEB Changelog ---

echo "  debian/changelog"
{
    echo "qlog (${QLOG_VERSION}-1) UNRELEASED; urgency=low"
    echo "${ENTRIES}" | sed 's/^-/  */'
    echo ""
    echo " -- foldynl <foldyna@gmail.com>  ${RELEASE_DATE_DEB}"
    echo ""
    cat "${DEB_CHANGELOG}"
} > "${TMPFILE}"
cp "${TMPFILE}" "${DEB_CHANGELOG}"

# --- RPM Changelog ---

echo "  packaging/rpm/qlog.spec"
{
    echo "* ${RELEASE_DATE_RPM} Ladislav Foldyna - ${QLOG_VERSION}-1"
    echo "${ENTRIES}"
    echo ""
} > "${TMPFILE}"
sed -i -e "/%changelog/{r ${TMPFILE}" -e '}' "${RPM_SPEC}"

# --- Appstream Metainfo ---

echo "  res/io.github.foldynl.QLog.metainfo.xml"
{
    echo "    <release version=\"${QLOG_VERSION}\" date=\"${RELEASE_DATE_ISO}\">"
    echo "      <description>"
    echo "        <ul>"
    echo "${ENTRIES}" | sed 's/^- //' | xml_escape | sed 's/^/          <li>/; s/$/<\/li>/'
    echo "        </ul>"
    echo "      </description>"
    echo "    </release>"
} > "${TMPFILE}"
sed -i -e "/  <releases>/{r ${TMPFILE}" -e '}' "${METAINFO}"
appstreamcli validate "${METAINFO}"

# --- Qt Installer files ---

echo "  installer/packages/.../package.xml"
sed -i "s/<Version>.*<\/Version>/<Version>${QLOG_VERSION}-1<\/Version>/" "${INSTALLER_PKG}"
sed -i "s/<ReleaseDate>.*<\/ReleaseDate>/<ReleaseDate>${RELEASE_DATE_ISO}<\/ReleaseDate>/" "${INSTALLER_PKG}"

echo "  installer/config/config.xml"
sed -i "s/<Version>.*<\/Version>/<Version>${QLOG_VERSION}<\/Version>/" "${INSTALLER_CFG}"

# --- Summary ---

echo ""
echo "Release ${QLOG_VERSION} finalized. Changed files:"
echo "  - ${QLOG_PRO}"
echo "  - ${CHANGELOG}"
echo "  - ${DEB_CHANGELOG}"
echo "  - ${RPM_SPEC}"
echo "  - ${METAINFO}"
echo "  - ${INSTALLER_PKG}"
echo "  - ${INSTALLER_CFG}"

# --- Commit ---

echo ""
read -r -p "Commit changes? [y/N] " confirm
if [[ ! "${confirm}" =~ ^[Yy]$ ]]; then
    echo "Aborted."
    exit 0
fi

git commit -a -m "Preparation for release ${QLOG_VERSION}"
echo "Changes committed."

# --- Push, merge to master ---

BRANCH=$(git rev-parse --abbrev-ref HEAD)
echo ""
echo "Pushing ${BRANCH}..."
git push origin "${BRANCH}"

echo "Switching to master and merging ${BRANCH}..."
git checkout master
git pull
git merge "${BRANCH}"

# --- Tag and push ---

TAG="v${QLOG_VERSION}"

echo ""
echo "Wait for GitHub Actions to pass, then confirm."
echo ""
read -r -p "Tag and push ${TAG}? [y/N] " confirm
if [[ ! "${confirm}" =~ ^[Yy]$ ]]; then
    echo "Aborted. To tag manually later:"
    echo "  git tag -a ${TAG} -m \"Release ${QLOG_VERSION}\""
    echo "  git push --atomic origin master ${TAG}"
    exit 0
fi

git tag -a "${TAG}" -m "Release ${QLOG_VERSION}"
git push --atomic origin master "${TAG}"

echo ""
echo "Tag ${TAG} pushed. You can now create the GitHub release:"
echo "  ./devtools/deployment/create_github_release.sh"
