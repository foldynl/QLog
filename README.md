# QLog

QLog is an Amateur Radio logging application for Linux, Windows. It
is based on the Qt framework and uses SQLite as database backend.

QLogs aims to be as simple as possible, but to provide everything the operator expects from the log to be. This log is not currently focused on contests.

![Screenshot](https://foldynl.github.io/QLog/screens/qlog_main.png)

## Features

- Customizable GUI
- Rig control via Hamlib, Omnirig v1 (Windows only), Omnirig v2 (Windows only), TCI, FLRig
- Rotator control via Hamlib, PSTRotator
- HamQTH and QRZ.com callbook integration
- DX cluster integration
- **LoTW**, **eQSL**, **QRZ.com**, **Clublog**, **HRDLog.net**, **ON4KST Chat**, **Cloudlog/Wavelog** integration (**eQSL includes QSL pictures download**)
- **Secure Password Storage** for all services with password or security token
- **Online** and **Offline** map
- Club Member lookup
- CW Keyer Support - CWDaemon, FLDigi (all supported modes), Morse Over CAT, WinKey v1 or v2
- Bandmap
- CW Console
- WSJT-X integration
- Station Location Profile support
- Various station statistics
- Basic Awards support
- Basic Contest support
- Custom QSO Filters
- **NO** ads, **NO** user tracking, **NO** hidden telemetry - simply free and open-source
- SQLite backend.

### Supported OS
* Linux
* Windows 10/11 (64bit)
* MacOS (experimental - only for developers)

### Supported Rigs
* all supported by [Hamlib](https://hamlib.github.io/)
* all supported by [Omnirig v1](https://www.dxatlas.com/omnirig/) (Windows only)
* all supported by [Omnirig v2](https://www.hb9ryz.ch/omnirig/) (Windows only)
* all supported by [TCI](https://eesdr.com/en/software-en/software-en)
* all supported by [FLRig](http://www.w1hkj.com/flrig-help/supported_transceivers.html)

### Supported Rotators
* all supported by [Hamlib](https://hamlib.github.io/)

### Supported Keyers
* [CWDaemon](https://cwdaemon.sourceforge.net/)
* [FLDigi](http://www.w1hkj.com/)
* Morse Over CAT
* WinKey v1 or v2 compatible hardware

### Supported Secure Password Storage
* Linux: LibSecretKeyring, GnomeKeyring, Kwallet4, Kwallet5
* Windows: Windows Credential Store
* MacOS: macOS Keychain

### Third-party software
* [TQSL](http://www.arrl.org/tqsl-download) – optional, needed for LoTW support

For more details, screenshots etc, please, see [QLog Wiki](https://github.com/foldynl/QLog/wiki)

Please, used [QLog Issues](https://github.com/foldynl/QLog/issues) for reporting any issue or open a [discussion](https://github.com/foldynl/QLog/discussions).
You can also use [QLog mailing list](https://groups.io/g/qlog)


## Installation

### Minimum Hardware Requirements
- The recommended graphical resolution: 1920x1080
- CPU and memory: minimum requirements the same as for your OS
- Graphic Card with OpenGL support
- Serial connection if radio control is used

### Linux

Prerequisites:

- Installed Trusted QSL (Optional) - `sudo apt install trustedqsl` or from [ARRL](http://www.arrl.org/tqsl-download)

**DEB packages** for currently supported Ubuntu versions are available for amd64, arm64 platforms via [Ubuntu PPA](https://launchpad.net/~foldyna/+archive/ubuntu/qlog). Ubuntu users can use following commands:

`sudo add-apt-repository ppa:foldyna/qlog`

`sudo apt update`

`sudo apt install qlog`

Fedora **RPM packages** are available via GitHub [Releases](https://github.com/foldynl/QLog/releases/latest)

<a href='https://flathub.org/apps/io.github.foldynl.QLog'>   <img width='120' alt='Download on Flathub' src='https://dl.flathub.org/assets/badges/flathub-badge-en.png'/></a>

**Flatpak** package is available via [Flathub](https://flathub.org/apps/io.github.foldynl.QLog). The package contains built-in TrustedQSL.

The above packages are maintained by the application maintainer. However, there are many other unofficial packages available for other distributions — see the Unofficial Repositories on [QLog Wiki](https://github.com/foldynl/QLog/wiki).

### Windows

Prerequisites:

- Installed [Trusted QSL](http://www.arrl.org/tqsl-download) (Optional)
- Installed [Omnirig v1](https://www.dxatlas.com/omnirig/) (Optional)
- Installed [Omnirig v2](https://www.hb9ryz.ch/omnirig/) (Optional)

Installation package is available via GitHub [Releases](https://github.com/foldynl/QLog/releases) .

### MacOS

Official support ended. Only for developers.

## Compilation

### General

Prerequisites

- Installed Qt
- Installed [qtkeychain-devel](https://github.com/frankosterfeld/qtkeychain) library and headers
- Installed [OpenSSL-devel](https://wiki.openssl.org/index.php/Binaries) libraries and headers
- Installed [HamLib-devel](https://github.com/Hamlib/Hamlib/releases/latest) libraries and headers

`qmake` supports listed input parameters that affect the compilation process.

- `HAMLIBINCLUDEPATH` - the path to Hamlib Includes 
- `HAMLIBLIBPATH` - the path to Hamlib Library 
- `HAMLIBVERSION_MAJOR` - Hamlib version - major number (must be present if `pkg-config` cannot determine Hamlib version)
- `HAMLIBVERSION_MINOR` - Hamlib version - minor number (must be present if `pkg-config` cannot determine Hamlib version)
- `HAMLIBVERSION_PATCH` - Hamlib version - patch number (must be present if `pkg-config` cannot determine Hamlib version)
- `PTHREADINCLUDEPATH`  - the path to pthread Includes - needed for Windows Hamlib 4.5 and later. Leave empty if system libraries should be used.
- `PTHREADLIBPATH` - the path to pthread Library - needed for Windows Hamlib 4.5 and later. Leave empty if system libraries should be used.
- `QTKEYCHAININCLUDEPATH` - the path to QtKeyChain Includes 
- `QTKEYCHAINLIBPATH`- the path to QtKeyChain Library

Leave variables empty if system libraries and Hamlib version autodetect (calling `pkg-config`) should be used during compilation (for Windows, the parameter must be present)

An example of use:

`
C:/Qt/6.4.1/msvc2019_64/bin/qmake.exe C:\Users\devel\development\QLog\QLog.pro -spec win32-msvc "CONFIG+=qtquickcompiler" "HAMLIBINCLUDEPATH = C:\Users\devel\development\hamlib\include" "HAMLIBLIBPATH =  C:\Users\devel\development\hamlib\lib\gcc" "HAMLIBVERSION_MAJOR = 4" "HAMLIBVERSION_MINOR = 5" "HAMLIBVERSION_PATCH = 0" "QTKEYCHAININCLUDEPATH = C:\Users\devel\development\qtkeychain_build\include" "QTKEYCHAINLIBPATH = C:\Users\devel\development\qtkeychain_build\lib" && C:/Qt/Tools/QtCreator/bin/jom/jom.exe qmake_all
`

### Windows

Prerequisites

- [Visual Studio 2019](https://visualstudio.microsoft.com/vs/community/)
- QT with source codes (6.x, Qt Webengine, OpenSSL Toolkit)
- [Omnirig v1](https://www.dxatlas.com/omnirig/)
- [Omnirig v2](https://www.hb9ryz.ch/omnirig/)
- [Hamlib](https://github.com/Hamlib/Hamlib/releases)
  - hamlib-w64-4.5.5.exe is the latest
    - Need to run the following commands to "fix" the library *** Fix Paths if necessary ***
      CD "C:\Program Files\hamlib-w64-4.5.5\lib\msvc"
      "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.41.34120\bin\Hostx64\x64\link.exe" /lib /machine:X64 /def:libhamlib-4.def
      copy libhamlib-4.lib hamlib.lib
- Install C++ Module - https://learn.microsoft.com/en-us/vcpkg/get_started/get-started-vscode?pivots=shell-powershell

	cd C:\Program Files\Microsoft Visual Studio\2022\Community\VC\vcpkg
	vcpkg install pthreads
	vcpkg install qtkeychain-qt6

System Environmental Path Settings
	C:\Program Files\hamlib-w64-4.5.5\bin
	C:\QTTools\vcpkg\packages\qtkeychain-qt6_x64-windows\bin

Clone QLog Master Branch
In QT Creator Projects->Desktop Qt 6.8.0 MSVC2022 64Bit->Build Steps->Additional Arguments
	**** You need to update the paths accordingly ****
	**** Need to be on same line seperated by spaces ****
	"HAMLIBINCLUDEPATH='C:\Program Files\hamlib-w64-4.5.5\include'"
	"HAMLIBLIBPATH='C:\Program Files\hamlib-w64-4.5.5\lib\msvc'"
	"HAMLIBVERSION_MAJOR=4"
	"HAMLIBVERSION_MINOR=5"
	"HAMLIBVERSION_PATCH=5"
	"QTKEYCHAININCLUDEPATH=C:\QTTools\vcpkg\packages\qtkeychain-qt6_x64-windows\include"
	"QTKEYCHAINLIBPATH=C:\QTTools\vcpkg\packages\qtkeychain-qt6_x64-windows\lib"
	"PTHREADLIBPATH=C:\QTTools\vcpkg\packages\pthreads_x64-windows\lib"
	"PTHREADINCLUDEPATH=C:\QTTools\vcpkg\packages\pthreads_x64-windows\include"


### Linux

for Debian:

`sudo apt-get -y install qtbase5-dev qtchooser qt5-qmake qtbase5-dev-tools libsqlite3-dev libhamlib++-dev libqt5charts5-dev qttools5-dev-tools libqt5keychain1 qt5keychain-dev qtwebengine5-dev build-essential libqt5serialport5-dev pkg-config libqt5websockets5-dev`

for Debian (QT6):

`sudo apt-get -y install libhamlib-dev build-essential pkg-config qt6-base-dev qtkeychain-qt6-dev qt6-webengine-dev libqt6charts6-dev libqt6serialport6 libqt6webenginecore6-bin libqt6svg6-dev libgl-dev libqt6websockets6-dev qt6-serialport-dev libsqlite3-dev`

for Fedora:

`dnf install qt5-qtbase-devel qt5-qtwebengine-devel qt5-qtcharts-devel hamlib-devel qtkeychain-qt5-devel qt5-qtserialport-devel pkg-config qt5-qtwebsockets-devel libsqlite3x-devel`

for both:

`git clone https://github.com/foldynl/QLog.git`

`cd  QLog`

for Debian:

`qmake QLog.pro`

for Debian (QT6):

`qmake6 QLog.pro`

for Fedora:

`/usr/bin/qmake-qt5`

NOTE: if it is necessary then use `qmake` input parameters described above to affect compilation. The input parameter must be use in case when Hamlib or qtkeychain are compiled from their source code repos.

for all:

`make`

### MacOS

In order to build QLog on MacOS, following prerequisites must be satisfied.

1. [Xcode](#xcode) command line tools
2. [Homebrew](https://brew.sh)
3. [Qt](https://www.qt.io) with QtCreator

##### Xcode

Xcode command line tools can be installed by issuing a command in command terminal:

```
xcode-select --install
```

**N.B.:** This command doesn't install Xcode itself, however It will take some time to download and
install the tools anyway.

##### MacOS build

Last dependencies before building QLog are:

```
 brew install qt6
 brew link qt6 --force
 brew install hamlib
 brew link hamlib --force
 brew install qtkeychain
 brew install dbus-glib
 brew install brotli
 brew install icu4c
 brew install pkg-config
```

As soon as the steps above are finished, QLog source can be opened in QtCreator, configured, built and run.
QLog app (qlog.app) from the build artifacts folder can be later copied (`installed`) to `~/Applications` and
accessed via Spotlight search bar.

NOTE: if it is necessary then use `qmake` input parameters described above to affect compilation. The input parameter must be use in case when hamlib or qtkeychain is compiled from their source code repos.



## License

Copyright (C) 2020  Thomas Gatzweiler

Copyright (C) 2021-2025  Ladislav Foldyna

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
