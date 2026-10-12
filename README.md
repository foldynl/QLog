# QLog

QLog is a free, open-source amateur radio logbook for Linux, Windows and macOS. It brings together daily logging, station control, award tracking and QSL workflows in a customizable desktop interface.

QLog is built with Qt and stores logbook data locally in SQLite. It has no advertising, user tracking or hidden telemetry. Its primary focus is general logging, with basic tools for occasional contest operation.

[Downloads](https://github.com/foldynl/QLog/releases/latest) · [Documentation](https://github.com/foldynl/QLog/wiki) · [Discussions](https://github.com/foldynl/QLog/discussions)

![QLog main window](https://foldynl.github.io/QLog/screens/qlog_main.png)

## Features

### Logbook

- Customizable layout, station location profiles and QSO filters
- ADIF import and export, station statistics and club member lookup
- Worked and confirmed award tracking, including DXCC, WAS, WAZ, WPX, IOTA, POTA, SOTA and WWFF; see [Awards](https://github.com/foldynl/QLog/wiki/Awards) for the full scope
- Basic contest logging with exchange fields, serial numbers, duplicate checks and Cabrillo export; contest scoring and multipliers are not calculated

### Station operation

- Rig control through [Hamlib](https://hamlib.github.io/), [OmniRig v1](https://www.dxatlas.com/omnirig/) and [v2](https://www.hb9ryz.ch/omnirig/) (Windows only), [TCI](https://eesdr.com/en/software-en/software-en) and [FLRig](http://www.w1hkj.com/flrig-help/supported_transceivers.html)
- Rotator control through Hamlib and [PSTRotator](https://pstrotator.com/)
- DX cluster, bandmap, online and offline maps
- WSJT-X and fldigi integration
- CW console and keying through [CWDaemon](https://cwdaemon.sourceforge.net/), [fldigi](http://www.w1hkj.com/), Morse over CAT and WinKey v1/v2 compatible hardware

Device support depends on the selected backend. Configuration details are documented under [Rig](https://github.com/foldynl/QLog/wiki/Rig), [Rotator](https://github.com/foldynl/QLog/wiki/Rotator) and [CW Console](https://github.com/foldynl/QLog/wiki/CW-Console).

### Online services and QSLs

- HamQTH and QRZ.com callbook lookup
- LoTW, eQSL, QRZ.com, Club Log, HRDLog.net, Cloudlog and Wavelog integration
- eQSL image downloads, QSL gallery and QSL card and label printing
- ON4KST chat integration
- Secure storage of service passwords and tokens using the Linux system keyring, Windows Credential Store or macOS Keychain

## Platforms and requirements

| Platform | Support |
| --- | --- |
| Linux | Native packages, Flatpak and AppImage; see installation options below |
| Windows | Windows 10/11, 64-bit |
| macOS | DMG builds are available |

A display resolution of 1920 × 1080 is recommended. Graphics support for OpenGL is required. Radio and accessory control require the appropriate serial, USB or network connection for the selected backend.

## Installation

### Linux

Use the Ubuntu PPA or Fedora repository for native installation on those distributions. Flatpak or AppImage are the recommended alternative for other distributions.

**Ubuntu — maintainer PPA**

Packages for amd64 and arm64 are available from the [QLog PPA](https://launchpad.net/~foldyna/+archive/ubuntu/qlog). Check the PPA for available Ubuntu releases.

```bash
sudo add-apt-repository ppa:foldyna/qlog
sudo apt update
sudo apt install qlog
```

**Fedora — distribution repository or GitHub Releases**

```bash
sudo dnf install qlog
```

Alternatively, install an RPM from [GitHub Releases](https://github.com/foldynl/QLog/releases/latest) matching your Fedora release and architecture. See the [Fedora package page](https://packages.fedoraproject.org/pkgs/qlog/qlog/) for repository versions.

**Flatpak — Flathub**

Install QLog from [Flathub](https://flathub.org/apps/io.github.foldynl.QLog). With Flathub configured:

```bash
flatpak install flathub io.github.foldynl.QLog
```

The Flatpak package includes TrustedQSL.

**Other packages**

RPM packages and AppImage builds are available from [GitHub Releases](https://github.com/foldynl/QLog/releases/latest). Choose an artifact matching your distribution and architecture. To run an AppImage, make the downloaded file executable and launch it.

Distribution packages are also available for Debian forky and openSUSE Tumbleweed:

| Distribution | Installation |
| --- | --- |
| Debian forky | `sudo apt install qlog` |
| openSUSE Tumbleweed | `sudo zypper install QLog` |

These packages are maintained by their respective distributions. Further packaging options are listed in the [wiki](https://github.com/foldynl/QLog/wiki/Unofficial-Repositories) and on [Repology](https://repology.org/project/qlog/versions).

For LoTW uploads, install [TrustedQSL](http://www.arrl.org/tqsl-download) separately unless it is included in your package. On Ubuntu and Debian, use `sudo apt install trustedqsl`; on Fedora, use `sudo dnf install trustedqsl`.

### Windows

Download the 64-bit installer from [GitHub Releases](https://github.com/foldynl/QLog/releases/latest).

- Install the [Microsoft Visual C++ Redistributable for x64](https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist?view=msvc-170#latest-supported-redistributable-version).
- Install [TrustedQSL](http://www.arrl.org/tqsl-download) if you use LoTW uploads.
- Install OmniRig v1 or v2 only if you select that rig-control backend.

### macOS

DMG builds are published in [aa5sh/QLog Releases](https://github.com/aa5sh/QLog/releases). Build and packaging issues for those DMGs should be reported to their maintainer.

## Documentation and support

The [QLog Wiki](https://github.com/foldynl/QLog/wiki) covers configuration, operating workflows and screenshots. Start with [Initial Setup](https://github.com/foldynl/QLog/wiki/Initial-Setup) for station and device profiles, or [Main Window](https://github.com/foldynl/QLog/wiki/Main-Window) for the operating interface.

- [Issues](https://github.com/foldynl/QLog/issues): bug reports and feature requests. Include the QLog version, OS, package source and steps to reproduce.
- [Discussions](https://github.com/foldynl/QLog/discussions): operating questions and configuration help.

Support in this repository covers the official QLog application. For installation or packaging problems with distribution or community builds, contact the package maintainer. Forks and modified versions are supported by their own maintainers; reproduce an application issue with an official, unmodified version before reporting it here.

## Building from source

QLog uses C++11, Qt5 or Qt6, and qmake. Dependencies, platform-specific build steps and qmake options are documented in [Building QLog](BUILDING.md). Container builds and installer generation are covered in [Packaging](packaging/README.md).

## License

QLog is licensed under the [GNU General Public License, version 3 or later](LICENSE), and is distributed without warranty. Bundled components retain their respective licenses.

Copyright (C) 2020 Thomas Gatzweiler

Copyright (C) 2021–2026 Ladislav Foldyna
