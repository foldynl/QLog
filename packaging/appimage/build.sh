#!/usr/bin/env bash

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
PACKAGING="$ROOT/packaging/appimage"
RUNTIME="$PACKAGING/runtime"
CHECKSUMS="$PACKAGING/checksums.sha256"

# shellcheck source=versions.sh
source "$PACKAGING/versions.sh"

NATIVE_ARCH="$(uname -m)"
JOBS="${JOBS:-$(nproc)}"

case "$NATIVE_ARCH" in
    x86_64|amd64)
        APPIMAGE_ARCH="x86_64"
        ;;
    aarch64|arm64)
        APPIMAGE_ARCH="aarch64"
        ;;
    *)
        echo "Unsupported architecture: $NATIVE_ARCH" >&2
        exit 1
        ;;
esac

WORK="$ROOT/build-appimage/$APPIMAGE_ARCH"
PREFIX="$WORK/deps"
DOWNLOADS="$WORK/downloads"
HAMLIB_SOURCE="$WORK/hamlib-${HAMLIB_VERSION}"
QTKEYCHAIN_SOURCE="$WORK/qtkeychain-${QTKEYCHAIN_VERSION}"
QTKEYCHAIN_ARCHIVE="$DOWNLOADS/qtkeychain-${QTKEYCHAIN_VERSION}.tar.gz"
QTKEYCHAIN_BUILD="$WORK/qtkeychain-${QTKEYCHAIN_VERSION}-build"
QLOG_BUILD="$WORK/qlog"
APPDIR="$WORK/AppDir"
TOOLS="$WORK/tools"
DIST="$ROOT/dist/appimage"

mkdir -p \
    "$PREFIX" \
    "$DOWNLOADS" \
    "$TOOLS" \
    "$DIST"

expected_checksum()
{
    local name="$1"

    awk -v name="$name" '$2 == name { print $1; exit }' "$CHECKSUMS"
}

download()
{
    local url="$1"
    local target="$2"
    local checksum
    local part="${target}.part"

    checksum="$(expected_checksum "$(basename "$target")")"

    if [[ -z "$checksum" ]]; then
        echo "Error: no checksum declared for $(basename "$target")." >&2
        exit 1
    fi

    if [[ -s "$target" ]]; then
        if echo "$checksum  $target" | sha256sum --check --status; then
            return
        fi

        echo "Discarding cached file with an invalid checksum: $target" >&2
        rm -f "$target"
    fi

    echo "Downloading $(basename "$target")"
    rm -f "$part"
    curl --fail --location --retry 3 "$url" -o "$part"

    if ! echo "$checksum  $part" | sha256sum --check --status; then
        rm -f "$part"
        echo "Error: checksum verification failed for $(basename "$target")." >&2
        exit 1
    fi

    mv "$part" "$target"
}

find_library()
{
    local name="$1"

    ldconfig -p | awk -v name="$name" '
        $1 == name && path == "" { path = $NF }
        END { print path }
    '
}

bundle_package_runtime()
{
    local package="$1"
    local source
    local relative
    local destination
    local multiarch
    local count=0

    multiarch="$(dpkg-architecture -qDEB_HOST_MULTIARCH)"

    while IFS= read -r source; do
        case "$source" in
            *.so|*.so.*|*.chk)
                if [[ ! -f "$source" ]]; then
                    echo "Error: $package runtime file not found: $source" >&2
                    exit 1
                fi

                case "$source" in
                    /usr/lib/"$multiarch"/*)
                        relative="${source#/usr/lib/"$multiarch"/}"
                        ;;
                    /lib/"$multiarch"/*)
                        relative="${source#/lib/"$multiarch"/}"
                        ;;
                    *)
                        relative="$(basename "$source")"
                        ;;
                esac

                destination="$APPDIR/usr/lib/$relative"
                mkdir -p "$(dirname "$destination")"
                cp -v "$source" "$destination"
                count=$((count + 1))
                ;;
        esac
    done < <(dpkg-query -L "$package")

    if [[ $count -eq 0 ]]; then
        echo "Error: no runtime files found in package $package." >&2
        exit 1
    fi
}

BUILD_PLATFORM_ID="$(. /etc/os-release && printf '%s:%s' "$ID" "$VERSION_ID")"
export QT_SELECT=qt5
QMAKE_BIN="$(command -v qmake)"
QT_VERSION="$("$QMAKE_BIN" -query QT_VERSION)"

if [[ "$QT_VERSION" != 5.* ]]; then
    echo "Error: the AppImage build requires Qt 5, found Qt $QT_VERSION." >&2
    exit 1
fi

QTKEYCHAIN_LIBRARY="$PREFIX/lib/libqt5keychain.so"
DEPS_ID="platform=${BUILD_PLATFORM_ID};qt=${QT_VERSION};hamlib=${HAMLIB_VERSION}:$(expected_checksum "hamlib-${HAMLIB_VERSION}.tar.gz");qtkeychain=${QTKEYCHAIN_VERSION}:$(expected_checksum "qtkeychain-${QTKEYCHAIN_VERSION}.tar.gz")"
DEPS_STATE="$PREFIX/.qlog-appimage-dependencies"

if [[ -f "$DEPS_STATE" && "$(cat "$DEPS_STATE")" != "$DEPS_ID" ]]; then
    echo "Dependency build environment changed; resetting $PREFIX"
    rm -rf "$PREFIX"
    mkdir -p "$PREFIX"
fi

# Hamlib

HAMLIB_ARCHIVE="$DOWNLOADS/hamlib-${HAMLIB_VERSION}.tar.gz"
HAMLIB_STAMP="$PREFIX/.hamlib-${HAMLIB_VERSION}-verified"

if [[ ! -f "$HAMLIB_STAMP" || ! -x "$PREFIX/bin/rigctld" || ! -e "$PREFIX/lib/libhamlib.so" ]]; then
    download \
        "${HAMLIB_REPOSITORY}/releases/download/${HAMLIB_VERSION}/hamlib-${HAMLIB_VERSION}.tar.gz" \
        "$HAMLIB_ARCHIVE"

    rm -rf "$HAMLIB_SOURCE"
    tar -xzf "$HAMLIB_ARCHIVE" -C "$WORK"

    cd "$HAMLIB_SOURCE"
    ./configure \
        --prefix="$PREFIX" \
        --enable-shared \
        --disable-static

    make -j"$JOBS"
    make install
    touch "$HAMLIB_STAMP"
else
    echo "Using cached Hamlib ${HAMLIB_VERSION}"
fi

# QtKeychain

QTKEYCHAIN_STAMP="$PREFIX/.qtkeychain-${QTKEYCHAIN_VERSION}-verified"

if [[ ! -f "$QTKEYCHAIN_STAMP" || ! -e "$QTKEYCHAIN_LIBRARY" ]]; then
    download \
        "${QTKEYCHAIN_REPOSITORY}/archive/refs/tags/${QTKEYCHAIN_VERSION}.tar.gz" \
        "$QTKEYCHAIN_ARCHIVE"

    rm -rf "$QTKEYCHAIN_SOURCE" "$QTKEYCHAIN_BUILD"
    mkdir -p "$QTKEYCHAIN_SOURCE"
    tar -xzf "$QTKEYCHAIN_ARCHIVE" --strip-components=1 -C "$QTKEYCHAIN_SOURCE"

    cmake \
        -S "$QTKEYCHAIN_SOURCE" \
        -B "$QTKEYCHAIN_BUILD" \
        -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX="$PREFIX" \
        -DCMAKE_INSTALL_LIBDIR=lib \
        -DBUILD_WITH_QT5=ON \
        -DBUILD_SHARED_LIBS=ON \
        -DBUILD_TRANSLATIONS=ON \
        -DBUILD_TESTING=OFF \
        -DBUILD_TEST_APPLICATION=OFF \
        -DBUILD_QTQUICK_DEMO=OFF

    cmake --build "$QTKEYCHAIN_BUILD" --parallel "$JOBS"
    cmake --install "$QTKEYCHAIN_BUILD"
    touch "$QTKEYCHAIN_STAMP"
else
    echo "Using cached QtKeychain ${QTKEYCHAIN_VERSION}"
fi

printf '%s\n' "$DEPS_ID" > "$DEPS_STATE"

# QLog

export PKG_CONFIG_PATH="$PREFIX/lib/pkgconfig:${PKG_CONFIG_PATH:-}"
export LD_LIBRARY_PATH="$PREFIX/lib:${LD_LIBRARY_PATH:-}"

rm -rf "$QLOG_BUILD"
mkdir -p "$QLOG_BUILD"
cd "$QLOG_BUILD"
"$QMAKE_BIN" "$ROOT/QLog.pro" \
    CONFIG+=release \
    CONFIG-=debug \
    PREFIX=/usr \
    HAMLIBINCLUDEPATH="$PREFIX/include" \
    HAMLIBLIBPATH="$PREFIX/lib" \
    QTKEYCHAININCLUDEPATH="$PREFIX/include" \
    QTKEYCHAINLIBPATH="$PREFIX/lib"

make -j"$JOBS"

# AppDir

rm -rf "$APPDIR"
mkdir -p "$APPDIR"

make install INSTALL_ROOT="$APPDIR"
cp -v "$PREFIX/bin/rigctld" "$APPDIR/usr/bin/rigctld"

# linuxdeploy

LINUXDEPLOY="$TOOLS/linuxdeploy-${APPIMAGE_ARCH}.AppImage"
QT_PLUGIN="$TOOLS/linuxdeploy-plugin-qt-${APPIMAGE_ARCH}.AppImage"

download \
    "${LINUXDEPLOY_REPOSITORY}/releases/download/continuous/linuxdeploy-${APPIMAGE_ARCH}.AppImage" \
    "$LINUXDEPLOY"
download \
    "${LINUXDEPLOY_QT_REPOSITORY}/releases/download/continuous/linuxdeploy-plugin-qt-${APPIMAGE_ARCH}.AppImage" \
    "$QT_PLUGIN"

chmod +x "$LINUXDEPLOY" "$QT_PLUGIN"

DEPLOY_LIBRARIES=()

while IFS= read -r library; do
    [[ -z "$library" || "$library" == \#* ]] && continue
    library_path="$(find_library "$library")"

    if [[ ! -f "$library_path" ]]; then
        echo "Error: declared runtime library was not found: $library" >&2
        exit 1
    fi

    DEPLOY_LIBRARIES+=(--library "$library_path")
done < "$RUNTIME/bundle-libraries.txt"

export QMAKE="$QMAKE_BIN"
export APPIMAGE_EXTRACT_AND_RUN=1
export ARCH="$APPIMAGE_ARCH"
export PATH="$TOOLS:$PATH"

cd "$WORK"
"$LINUXDEPLOY" \
    --appdir "$APPDIR" \
    --executable "$APPDIR/usr/bin/qlog" \
    --executable "$APPDIR/usr/bin/rigctld" \
    --library "$(readlink -f "$PREFIX/lib/libhamlib.so")" \
    --library "$(readlink -f "$QTKEYCHAIN_LIBRARY")" \
    "${DEPLOY_LIBRARIES[@]}" \
    --desktop-file "$APPDIR/usr/share/applications/qlog.desktop" \
    --icon-file "$APPDIR/usr/share/icons/hicolor/256x256/apps/qlog.png" \
    --plugin qt

if [[ ! -x "$APPDIR/usr/libexec/QtWebEngineProcess" ]]; then
    echo "Error: linuxdeploy did not bundle QtWebEngineProcess." >&2
    exit 1
fi

# NSS and OpenSSL runtime modules

# Complete package families listed here contain modules loaded through dlopen().
# Keeping each package intact prevents a host module from being paired with an
# older bundled core library.
while IFS= read -r package; do
    [[ -z "$package" || "$package" == \#* ]] && continue
    bundle_package_runtime "$package"
done < "$RUNTIME/bundle-packages.txt"

rm -f "$APPDIR/AppRun"
install -m 0755 "$RUNTIME/AppRun" "$APPDIR/AppRun"
mkdir -p "$APPDIR/usr/lib/gio/modules"

ln -sfn libssl.so.3 "$APPDIR/usr/lib/libssl.so"
ln -sfn libcrypto.so.3 "$APPDIR/usr/lib/libcrypto.so"

find "$WORK" -maxdepth 1 -type f -name '*.AppImage' -delete
"$LINUXDEPLOY" \
    --appdir "$APPDIR" \
    --output appimage

# Publish

VERSION="$(sed -n 's/^VERSION *= *//p' "$ROOT/QLog.pro" | head -n 1)"

if [[ -z "$VERSION" ]]; then
    echo "Error: QLog version was not found in $ROOT/QLog.pro." >&2
    exit 1
fi

mapfile -t APPIMAGES < <(find "$WORK" -maxdepth 1 -name '*.AppImage' -type f -print)

if [[ ${#APPIMAGES[@]} -ne 1 ]]; then
    echo "Error: expected one AppImage in $WORK, found ${#APPIMAGES[@]}." >&2
    exit 1
fi

OUTPUT="$DIST/QLog-${VERSION}-${APPIMAGE_ARCH}.AppImage"
mv -f "${APPIMAGES[0]}" "$OUTPUT"

echo
echo "Created:"
ls -lh "$OUTPUT"
