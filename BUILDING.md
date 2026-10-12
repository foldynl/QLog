# Building QLog

QLog uses C++11 and qmake, with support for Qt5 and Qt6. Keep generated files in a separate build directory under `build/`. Use separate directories when switching Qt versions or compiler toolchains.

For container builds, AppImage and RPM packaging, and Windows installer generation, see [Packaging](packaging/README.md).

## Dependencies

Install a C++11 compiler, make or jom, pkg-config where available, and the development headers and libraries for:

- Qt, including Charts, WebEngine, Serial Port, WebSockets and the SQLite SQL driver
- [Hamlib](https://github.com/Hamlib/Hamlib/releases)
- [QtKeychain](https://github.com/frankosterfeld/qtkeychain), built for the selected Qt major version
- OpenSSL, SQLite and zlib
- pthreads for Windows builds using Hamlib 4.5 or later

The complete Qt module list is declared in [QLog.pro](QLog.pro). On Windows, the project compiles SQLite from the Qt source tree, so the Qt installation must include its sources.

## Linux

### Debian and Ubuntu dependencies

Package names and availability depend on the distribution release. The following lists cover the Qt5 and Qt6 development environments.

**Qt5**

```bash
sudo apt update
sudo apt install build-essential pkg-config libhamlib-dev libsqlite3-dev \
    libssl-dev zlib1g-dev qtbase5-dev qt5-qmake qtbase5-dev-tools \
    qttools5-dev-tools qtwebengine5-dev libqt5charts5-dev \
    libqt5serialport5-dev libqt5websockets5-dev libqt5svg5-dev \
    qt5keychain-dev libqt5sql5-sqlite
```

**Qt6 — Debian**

```bash
sudo apt update
sudo apt install build-essential pkg-config libhamlib-dev libsqlite3-dev \
    libssl-dev zlib1g-dev qt6-base-dev qt6-webengine-dev qt6-charts-dev \
    qt6-serialport-dev qt6-websockets-dev libqt6svg6-dev libgl-dev \
    qtkeychain-qt6-dev libqt6sql6-sqlite
```

On Ubuntu 22.04/24.04, replace `qt6-charts-dev`, `qt6-serialport-dev` and `qt6-websockets-dev` with `libqt6charts6-dev`, `libqt6serialport6-dev` and `libqt6websockets6-dev`, respectively.

### Fedora dependencies

**Qt5**

```bash
sudo dnf install gcc-c++ make pkg-config hamlib-devel sqlite-devel \
    openssl-devel zlib-devel qt5-qtbase-devel qt5-qtwebengine-devel \
    qt5-qtcharts-devel qt5-qtserialport-devel qt5-qtwebsockets-devel \
    qtkeychain-qt5-devel
```

### Configure and build

Clone the repository and create a build directory:

```bash
git clone https://github.com/foldynl/QLog.git
cd QLog
mkdir -p build/qt5
cd build/qt5
```

Select the qmake executable for your Qt installation:

| Environment | Configure command |
| --- | --- |
| Debian or Ubuntu, Qt5 | `qmake ../../QLog.pro` |
| Fedora, Qt5 | `/usr/bin/qmake-qt5 ../../QLog.pro` |
| Debian or Ubuntu, Qt6 | `qmake6 ../../QLog.pro` |

For Qt6, use `build/qt6` instead of `build/qt5`. Then build and run from the selected build directory:

```bash
make -j"$(nproc)"
./qlog
```

Use the qmake options below if libraries are installed outside the system paths or the Hamlib version cannot be detected through pkg-config.

## Windows

Use a 64-bit MSVC toolchain and matching 64-bit dependencies:

- Visual Studio 2022 with C++ development tools
- Qt6 with an MSVC kit, WebEngine and Qt sources
- Hamlib headers, DLLs and MSVC import libraries
- QtKeychain, pthreads and zlib; these can be installed through vcpkg
- OpenSSL headers and libraries

### Dependencies and library paths

From a configured vcpkg installation:

```bat
vcpkg install pthreads:x64-windows qtkeychain-qt6:x64-windows zlib:x64-windows
```

If your Hamlib package supplies `libhamlib-4.def` but lacks the required MSVC import library, run the following in a Visual Studio x64 developer command prompt. Set `HAMLIB_ROOT` to the installed Hamlib directory first:

```bat
cd /d "%HAMLIB_ROOT%\lib\msvc"
link /lib /machine:X64 /def:libhamlib-4.def
copy libhamlib-4.lib hamlib.lib
```

Make the Hamlib, QtKeychain and OpenSSL runtime DLL directories available on `PATH` when launching a local build. Include any other dependency DLL directories required by your installation.

### Qt Creator

Open `QLog.pro`, select the Qt6 MSVC 64-bit kit and configure a build directory such as `build/windows`. Add the library paths and Hamlib version to the qmake build-step arguments.

The following example assumes Hamlib 4.7.2 and a vcpkg installation under `C:\dev`. Replace paths and version numbers with those of your installed dependencies. Pass each quoted assignment as one argument:

```text
"HAMLIBINCLUDEPATH=C:\dev\hamlib-w64-4.7.2\include"
"HAMLIBLIBPATH=C:\dev\hamlib-w64-4.7.2\lib\msvc"
"HAMLIBVERSION_MAJOR=4"
"HAMLIBVERSION_MINOR=7"
"HAMLIBVERSION_PATCH=2"
"QTKEYCHAININCLUDEPATH=C:\dev\vcpkg\packages\qtkeychain-qt6_x64-windows\include"
"QTKEYCHAINLIBPATH=C:\dev\vcpkg\packages\qtkeychain-qt6_x64-windows\lib"
"PTHREADINCLUDEPATH=C:\dev\vcpkg\packages\pthreads_x64-windows\include"
"PTHREADLIBPATH=C:\dev\vcpkg\packages\pthreads_x64-windows\lib"
"ZLIBINCLUDEPATH=C:\dev\vcpkg\packages\zlib_x64-windows\include"
"ZLIBLIBPATH=C:\dev\vcpkg\packages\zlib_x64-windows\lib"
"OPENSSLINCLUDEPATH=C:\dev\openssl\include"
"OPENSSLLIBPATH=C:\dev\openssl\lib"
```

Enter the arguments on one line, separated by spaces. OmniRig v1 or v2 is needed at runtime only when using the corresponding rig-control backend.

### Command-line builds

From a Visual Studio x64 developer command prompt with the Qt and jom executables on `PATH`, configure from a build directory:

```bat
mkdir build\windows
cd build\windows
qmake ..\..\QLog.pro -spec win32-msvc
jom
```

Add the same dependency assignments shown above to the qmake command as required by your installation.

For the repository's build and deployment script, adjust the tool and dependency paths in [make.bat](packaging/windows/make.bat), then run it from the repository root:

```bat
packaging\windows\make.bat release
```

Use `all` instead of `release` to rebuild and generate an installer. Installer generation also requires the Qt Installer Framework. The script builds under `build/deployment` and writes installers to `dist/windows`.

## macOS

Install Xcode command line tools, [Homebrew](https://brew.sh) and Qt Creator.

```bash
xcode-select --install
brew install qt6 hamlib qtkeychain dbus-glib brotli icu4c pkg-config
```

Configure Qt Creator to use the Homebrew Qt6 installation. Open `QLog.pro`, select the matching kit and set a build directory under `build/`. Use the qmake options below if dependency paths are not detected.

To expose the Homebrew installations on `PATH`, use `brew link qt6 --force` and `brew link hamlib --force`, or configure explicit kit and library paths.

After building, `qlog.app` can be run from the build directory or copied to `~/Applications`. For distributable packages, see the [community macOS releases](https://github.com/aa5sh/QLog/releases).

## qmake options

Supply overrides as qmake command-line assignments. Leave path overrides unset to use system locations. When pkg-config cannot determine the Hamlib version, supply all three version components; Windows builds normally require them explicitly.

| Option | Purpose |
| --- | --- |
| `HAMLIBINCLUDEPATH` | Hamlib header directory |
| `HAMLIBLIBPATH` | Hamlib library directory |
| `HAMLIBVERSION_MAJOR` | Hamlib major version |
| `HAMLIBVERSION_MINOR` | Hamlib minor version |
| `HAMLIBVERSION_PATCH` | Hamlib patch version |
| `PTHREADINCLUDEPATH` | pthread header directory for Windows Hamlib |
| `PTHREADLIBPATH` | pthread library directory for Windows Hamlib |
| `QTKEYCHAININCLUDEPATH` | QtKeychain header directory |
| `QTKEYCHAINLIBPATH` | QtKeychain library directory |
| `ZLIBINCLUDEPATH` | zlib header directory |
| `ZLIBLIBPATH` | zlib library directory |
| `OPENSSLINCLUDEPATH` | OpenSSL header directory |
| `OPENSSLLIBPATH` | OpenSSL library directory |
| `PREFIX` | Linux installation prefix, default `/usr` |

For example, from `build/qt6`, using a custom Hamlib installation:

```bash
qmake6 ../../QLog.pro \
    "HAMLIBINCLUDEPATH=/opt/hamlib/include" \
    "HAMLIBLIBPATH=/opt/hamlib/lib"
```

The matching runtime libraries must also be available to the application. Header and linker paths alone do not configure runtime library lookup.
