# QLog packaging builds

The packaging builds run in containers and write their results below `dist/`.
Podman is used by default, but Docker is also supported. All distro-specific
dependencies are installed inside the build images.

The AppImage build uses Qt 6. The RPM build deliberately follows the existing
Qt 5 `BuildRequires` in `rpm/qlog.spec`.

## AppImage

Debian build dependencies are declared in `appimage/packages.txt`. Complete
runtime package families loaded through `dlopen()` are declared separately in
`appimage/runtime/bundle-packages.txt`; additional dynamic libraries are in
`appimage/runtime/bundle-libraries.txt`. This prevents a bundled core library
from loading a newer, ABI-incompatible module from the host. These lists name
components, not their individual files: package contents and their dependency
closure are discovered during every build.

Hamlib, QtKeychain, linuxdeploy, and the Qt deployment plugin are pinned by
version and SHA-256 checksum. Hamlib and QtKeychain are cached per architecture
below `build-appimage/`. QLog itself and AppDir are rebuilt from scratch.

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
not reliably executable through QEMU/binfmt. Set `JOBS` to limit parallel
compilation.

Artifacts are written as:

```text
dist/appimage/QLog-VERSION-x86_64.AppImage
dist/appimage/QLog-VERSION-aarch64.AppImage
```

## RPM

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

Do not invoke the wrappers with `sudo`; both engines run the package build with
the invoking host user's UID and GID. The wrappers deliberately reject
architecture arguments and build only for the architecture reported by the
host. To produce both architectures, run the same scripts once on an x86_64
system and once on an ARM64 system.

Build-container images are temporary and are removed on success, failure, or
interruption. The small Fedora/Debian base images remain in the selected
engine's cache because subsequent builds reuse them.

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

Use native runners for both architectures. The same build command can be used
in a matrix; only the runner and its native Docker platform change:

```yaml
strategy:
  matrix:
    include:
      - arch: x86_64
        runner: ubuntu-24.04
        platform: linux/amd64
      - arch: aarch64
        runner: ubuntu-24.04-arm
        platform: linux/arm64

runs-on: ${{ matrix.runner }}
```

Do not add QEMU setup to these jobs. GitHub-hosted jobs can call the same
wrappers with `CONTAINER_ENGINE=docker`. The architecture must come from the
selected runner, not from an emulated container or a build-script argument.

## All default builds

The aggregate build creates native RPMs for Fedora 43 and 44, then creates a
native AppImage for the current host:

```bash
./packaging/build-all.sh
CONTAINER_ENGINE=docker ./packaging/build-all.sh
```
