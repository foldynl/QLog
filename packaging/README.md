# QLog packaging builds

The packaging builds run in temporary containers and write their results below
`dist/`. Podman is used by default, but Docker is also supported. All
distro-specific dependencies are installed inside the build containers.

Compilation uses all CPUs reported by `nproc` inside the container. Set `JOBS`
to override this, for example `JOBS=8 ./packaging/build-all.sh`.

The AppImage, DEB, and RPM builds use Qt 5. Ubuntu 22.04's Qt 6 WebEngine 6.2.4
predates the QTBUG-101201 fix and can abort with multi-layout XKB configurations.

## AppImage

Ubuntu 22.04 build dependencies are declared in `appimage/packages.txt`. Complete
runtime package families loaded through `dlopen()` are declared separately in
`appimage/runtime/bundle-packages.txt`; additional dynamic libraries are in
`appimage/runtime/bundle-libraries.txt`. This prevents a bundled core library
from loading a newer, ABI-incompatible module from the host. These lists name
components, not their individual files: package contents and their dependency
closure are discovered during every build.

Hamlib, QtKeychain, linuxdeploy, and the Qt deployment plugin are pinned by
version and SHA-256 checksum. Hamlib and QtKeychain are cached per architecture
below `build-appimage/`. QLog itself and AppDir are rebuilt from scratch.
Manual dependency check and update commands are documented in
`../devtools/dependencies/README.md`.

The AppImage has a deliberate runtime boundary:

- Qt, NSS, OpenSSL, libsecret, and their runtime modules stay inside AppImage.
- Qt and GLib plugin search paths are isolated from host plugin directories.
- GNOME Keyring/Secret Service remains a host service reached through D-Bus.
- glibc, the loader, graphics drivers, display/audio APIs, and hardware-facing
  libraries remain host owned according to linuxdeploy's exclusion policy.

```bash
./packaging/appimage/build-container.sh
```

Use Docker instead of Podman with:

```bash
CONTAINER_ENGINE=docker ./packaging/appimage/build-container.sh
```

The wrapper always builds for the native host architecture. An x86_64 host
creates an x86_64 AppImage and an ARM64 host creates an aarch64 AppImage. It
does not offer emulated cross-architecture builds because AppImage tooling is
not reliably executable through QEMU/binfmt.

Artifacts are written as:

```text
dist/appimage/QLog-VERSION-x86_64.AppImage
dist/appimage/QLog-VERSION-aarch64.AppImage
```

## DEB

Select an Ubuntu release in `YY.MM` format; the default is `24.04`.
Dependencies come from `../debian/control` through `mk-build-deps`.
The build uses a temporary source copy, checks `debian/copyright` with `cme`,
and fails on lintian errors.

```bash
./packaging/deb/build-container.sh 24.04
```

DEBs, `.changes`, and `.buildinfo` files go to `dist/deb/ubuntu-VERSION/ARCH/`.
The native architecture is named `amd64` on x86_64 and `arm64` on ARM64.

## RPM

Select Fedora 43 or later; the default is `43`.
RPM build dependencies are declared only as `BuildRequires` in
`rpm/qlog.spec`. The RPM build image installs them with `dnf builddep`; the
package build then runs as the invoking host user.

```bash
./packaging/rpm/build-container.sh 43
CONTAINER_ENGINE=docker ./packaging/rpm/build-container.sh 43
```

The RPM wrapper follows the same native-only rule. Its optional argument selects
the Fedora version, not the CPU architecture.

Artifacts, including the source RPM, are written below:

```text
dist/rpm/fedora-VERSION/ARCH/
```

## Local package smoke tests

After building a DEB or RPM, run its wrapper again with `--smoke-test`:

```bash
./packaging/deb/build-container.sh 24.04 --smoke-test
./packaging/rpm/build-container.sh 43 --smoke-test
```

The test installs the existing package from `dist/` in a fresh container for
the selected release and checks `qlog --help`. It does not rebuild the package.

Do not invoke the wrappers with `sudo`; both engines run the package build with
the invoking host user's UID and GID. The wrappers deliberately reject
architecture arguments and build only for the architecture reported by the
host. To produce both architectures, run the same scripts once on an x86_64
system and once on an ARM64 system.

Build containers are removed on success, failure, or interruption. The small
Fedora/Ubuntu base images remain in the selected engine's cache because
subsequent builds reuse them.

## Windows

The native MSVC and Qt build scripts are in `windows/`. Adjust their tool and
dependency paths to match the local Windows development environment, then run:

```bat
packaging\windows\make.bat all
packaging\windows\tests.bat
```

The installer is written as:

```text
dist/windows/QLog-VERSION-x86_64.exe
```

The Windows build is intentionally not part of `build-all.sh`.

## GitHub Actions

The [AppImage workflow](../.github/workflows/appimage.yml) builds and smoke-tests
native x86_64 and aarch64 AppImages on `master` pushes or manual dispatch.
DEB and RPM validation runs locally. The separate
[compilation and unit-test workflow](../.github/workflows/c-cpp.yml) is unchanged.

## All default builds

Edit `UBUNTU_VERSIONS` and `FEDORA_VERSIONS` in
[build-all.conf](build-all.conf) to select the target releases. The file uses
Bash arrays; an empty array skips that package family.

The script builds and smoke-tests the configured DEBs and RPMs, then builds
an AppImage. It stops on failure and prints `SUCCESS` after completion.

An optional GPG key ID or fingerprint signs each new DEB, RPM, source RPM, and
AppImage with a detached `.asc` signature. Signing requires a clean checkout
at the local `vVERSION` tag matching `QLog.pro`. Without a key, builds use the
current checkout and remove stale signatures only for rebuilt artifacts:

```bash
./packaging/build-all.sh
./packaging/build-all.sh 0C671AC7DEBDC8AC0C349269C214374D823D58CE
```
