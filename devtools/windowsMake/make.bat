@echo off
setlocal EnableExtensions EnableDelayedExpansion

rem ============================================================
rem  Build + Make script for QLog (MSVC + Qt)
rem  Usage:
rem    %~nx0              -> release
rem    %~nx0 release      -> release
rem    %~nx0 clean        -> clean release
rem    %~nx0 rebuild      -> clean + release
rem    %~nx0 deploy       -> windeployqt + binarycreator
rem    %~nx0 all          -> rebuild + deploy
rem ============================================================
rem ============================================================
rem - The script is executed in cmd.exe on Windows.
rem - The Visual Studio environment init batch (VS_VCVARS) must exist.
rem   - It must support the selected target (VS_ARCH), e.g. x86_amd64 for x64 builds.
rem - The Qt MSVC kit root (QT_BASE) must exist and contain:
rem   - %QT_BASE%\bin\qmake.exe
rem   - %QT_BASE%\bin\windeployqt.exe
rem - The jom executable (JOM) must exist (used for qmake_all and Makefile.Release).
rem - The Qt Installer Framework bin directory (QTIFW_BIN) must exist and contain:
rem   - binarycreator.exe
rem - The QMake project file (PRO) must exist and be readable.
rem - The working/build directory must be writable (Makefile.Release will be generated).
rem
rem - Hamlib must be present under %DEVROOT%\hamlib-w64-%HAMLIBVERSION% derived from HAMLIBVERSION:
rem   - Expected headers:   %HAMLIBROOT%\include\
rem   - Expected libraries: %HAMLIBROOT%\lib\msvc\
rem
rem - vcpkg packages root (VCPKG_PACKAGES) must exist and contain required packages:
rem   - %VCPKG_PACKAGES%\%QTKEYCHAIN_PKG%\include  and  ...\lib
rem   - %VCPKG_PACKAGES%\%PTHREAD_PKG%\include     and  ...\lib
rem   - %VCPKG_PACKAGES%\%ZLIB_PKG%\include        and  ...\lib
rem
rem ============================================================

set "ROOT=%~dp0"

if "%ROOT:~-1%"=="\" set "ROOT=%ROOT:~0,-1%"
for %%I in ("%ROOT%\..\..\..") do set "DEVROOT=%%~fI"

rem === CONFIGURATION ===

rem -- VC Compiler Settings
set "VS_VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat"

rem -- QT Settings
set "QT_BASE=C:\Qt\6.10.2\msvc2022_64"
set "QTIFW_BIN=C:\Qt\Tools\QtInstallerFramework\4.6\bin"
set "JOM=C:\Qt\Tools\QtCreator\bin\jom\jom.exe"

rem -- Project Settings
set "PROJECT_BASE=%DEVROOT%\QLog"
for /f "tokens=3" %%V in ('findstr /R /C:"^VERSION *= *" "%PROJECT_BASE%\QLog.pro"') do set "QLOG_VERSION=%%V"
set "INSTALLER_BASE=%DEVROOT%\qlog_build\qlog-installer-%QLOG_VERSION%"
set "INSTALLER_SEQ=0"
if exist "%INSTALLER_BASE%.exe" (
  :seq_loop
  set /a INSTALLER_SEQ+=1
  if exist "%INSTALLER_BASE%-!INSTALLER_SEQ!.exe" goto :seq_loop
)
if %INSTALLER_SEQ%==0 (
  set "INSTALLER_OUT=%INSTALLER_BASE%.exe"
) else (
  set "INSTALLER_OUT=%INSTALLER_BASE%-!INSTALLER_SEQ!.exe"
)

rem -- Libs Settings
set "VCPKG_PACKAGES=%DEVROOT%\vcpkg\packages"
set "VCPKG_QTKEYCHAIN_PKG=qtkeychain-qt6_x64-windows"
set "VCPKG_PTHREAD_PKG=pthreads_x64-windows"
set "VCPKG_ZLIB_PKG=zlib_x64-windows"

rem -- Hamlib Settings
set "HAMLIBVERSION_MAJOR=4"
set "HAMLIBVERSION_MINOR=7"
set "HAMLIBVERSION_PATCH=0"

rem === END OF CONFIGURATION ===

set "VS_ARCH=x86_amd64"

set "QMAKE=%QT_BASE%\bin\qmake.exe"
set "WINDEPLOYQT=%QT_BASE%\bin\windeployqt.exe"
set "BINARYCREATOR=%QTIFW_BIN%\binarycreator.exe"

set "PRO=%PROJECT_BASE%\QLog.pro"
set "BUILDROOT=%PROJECT_BASE%\build\deployment"

set "HAMLIBVERSION=%HAMLIBVERSION_MAJOR%.%HAMLIBVERSION_MINOR%.%HAMLIBVERSION_PATCH%"
set "HAMLIBROOT=%DEVROOT%\hamlib-w64-%HAMLIBVERSION%"
set "HAMLIBINCLUDEPATH=%HAMLIBROOT%\include"
set "HAMLIBLIBPATH=%HAMLIBROOT%\lib\msvc"
set "HAMLIBBINPATH=%HAMLIBROOT%\bin"

set "QTKEYCHAININCLUDEPATH=%VCPKG_PACKAGES%\%VCPKG_QTKEYCHAIN_PKG%\include"
set "QTKEYCHAINLIBPATH=%VCPKG_PACKAGES%\%VCPKG_QTKEYCHAIN_PKG%\lib"
set "QTKEYCHAINBINPATH=%VCPKG_PACKAGES%\%VCPKG_QTKEYCHAIN_PKG%\bin"

set "PTHREADINCLUDEPATH=%VCPKG_PACKAGES%\%VCPKG_PTHREAD_PKG%\include"
set "PTHREADLIBPATH=%VCPKG_PACKAGES%\%VCPKG_PTHREAD_PKG%\lib"

set "ZLIBINCLUDEPATH=%VCPKG_PACKAGES%\%VCPKG_ZLIB_PKG%\include"
set "ZLIBLIBPATH=%VCPKG_PACKAGES%\%VCPKG_ZLIB_PKG%\lib"
set "ZLIBBINPATH=%VCPKG_PACKAGES%\%VCPKG_ZLIB_PKG%\bin"


set "DEPLOY_DIR=%BUILDROOT%\installer\packages\de.dl2ic.qlog\data"
set "INSTALLER_CONFIG=%BUILDROOT%\installer\config\config.xml"
set "INSTALLER_PACKAGES=%BUILDROOT%\installer\packages"

set "OPENSSLROOT=%DEVROOT%\openssl-3.0\x64"

rem === Action ===

set "ACTION=%~1"
if /I "%ACTION%"=="" set "ACTION=release"
if not exist "%BUILDROOT%" mkdir "%BUILDROOT%"
pushd "%BUILDROOT%" >nul

rem === Initialize MSVC environment ===
echo === Initializing MSVC environment (%VS_ARCH%) ===
call "%VS_VCVARS%" %VS_ARCH%
if errorlevel 1 goto :fail

if /I "%ACTION%"=="clean"   goto :clean
if /I "%ACTION%"=="rebuild" goto :rebuild
if /I "%ACTION%"=="release" goto :release
if /I "%ACTION%"=="deploy"  goto :deploy
if /I "%ACTION%"=="all"     goto :all

echo Unknown action: %ACTION%
echo Usage: %~nx0 [release^|clean^|rebuild^|deploy^|all]
goto :fail

rem ============================================================
rem  BUILD STEPS
rem ============================================================
:qmake
echo === Running qmake ===
"%QMAKE%" "%PRO%" -spec win32-msvc ^
  "CONFIG+=qtquickcompiler" ^
  "HAMLIBINCLUDEPATH=%HAMLIBINCLUDEPATH%" ^
  "HAMLIBLIBPATH=%HAMLIBLIBPATH%" ^
  "HAMLIBVERSION_MAJOR=%HAMLIBVERSION_MAJOR%" ^
  "HAMLIBVERSION_MINOR=%HAMLIBVERSION_MINOR%" ^
  "HAMLIBVERSION_PATCH=%HAMLIBVERSION_PATCH%" ^
  "QTKEYCHAININCLUDEPATH=%QTKEYCHAININCLUDEPATH%" ^
  "QTKEYCHAINLIBPATH=%QTKEYCHAINLIBPATH%" ^
  "PTHREADINCLUDEPATH=%PTHREADINCLUDEPATH%" ^
  "PTHREADLIBPATH=%PTHREADLIBPATH%" ^
  "ZLIBINCLUDEPATH=%ZLIBINCLUDEPATH%" ^
  "ZLIBLIBPATH=%ZLIBLIBPATH%" ^
  "OPENSSLINCLUDEPATH=%OPENSSLROOT%/include" ^
  "OPENSSLLIBPATH=%OPENSSLROOT%/lib"

if errorlevel 1 exit /b 10

echo === jom qmake_all ===
"%JOM%" qmake_all
if errorlevel 1 exit /b 11
exit /b 0

:clean
call :qmake
if errorlevel 1 goto :fail

echo === Cleaning (Makefile.Release) ===
"%JOM%" -f Makefile.Release clean
if errorlevel 1 goto :fail

del /F /Q "%BUILDROOT%\release\qlog.exe" >nul
rmdir /S /Q "%BUILDROOT%\installer" >nul
goto :ok

:release
call :qmake
if errorlevel 1 goto :fail

echo === Building Release (Makefile.Release) ===
"%JOM%" -f Makefile.Release
if errorlevel 1 goto :fail
goto :ok

:rebuild
call :qmake
if errorlevel 1 goto :fail

echo === Cleaning (Makefile.Release) ===
"%JOM%" -f Makefile.Release clean
if errorlevel 1 goto :fail

echo === Building Release (Makefile.Release) ===
"%JOM%" -f Makefile.Release
if errorlevel 1 goto :fail
goto :ok

rem ============================================================
rem  DEPLOYMENT STEPS
rem ============================================================
:deploy
echo === Deployment ===

rem Ensure tools are reachable (only for this process)
set "PATH=%QT_BASE%\bin;%QTIFW_BIN%;%PATH%"

if not exist "%BUILDROOT%\release\qlog.exe" (
  echo ERROR: "%BUILDROOT%\release\qlog.exe" not found.
  echo Check DEPLOY_DIR: "%DEPLOY_DIR%"
  goto :fail
)

robocopy "%PROJECT_BASE%\installer" "%BUILDROOT%\installer" /MIR
if errorlevel 8 (
  echo ERROR: Cannot ROBOCOPY installer
  goto :fail
)

mkdir "%DEPLOY_DIR%"

rem  *****************
rem  Copy QLog Binary
rem  *****************
copy /Y "%BUILDROOT%\release\qlog.exe" "%DEPLOY_DIR%\qlog.exe"

if errorlevel 1 (
  echo ERROR: Cannot copy qlog.exe to "%DEPLOY_DIR%"
  goto :fail
)

rem  ***********
rem  Copy Hamlib
rem  ***********
copy /Y "%HAMLIBBINPATH%\*.dll" "%DEPLOY_DIR%"

if errorlevel 1 (
  echo ERROR: Cannot copy Hamlib DLL to "%DEPLOY_DIR%"
  goto :fail
)

copy /Y "%HAMLIBBINPATH%\rigctld.exe" "%DEPLOY_DIR%"

if errorlevel 1 (
  echo ERROR: Cannot copy rigctld.exe to "%DEPLOY_DIR%"
  goto :fail
)

rem  ****************
rem  Copy QtKeychain
rem  ****************
copy /Y "%QTKEYCHAINBINPATH%\*.dll" "%DEPLOY_DIR%"

if errorlevel 1 (
  echo ERROR: Cannot copy QTKeychain DLL to "%DEPLOY_DIR%"
  goto :fail
)

rem  ****************
rem  Copy zlib
rem  ****************
copy /Y "%ZLIBBINPATH%\*.dll" "%DEPLOY_DIR%"

if errorlevel 1 (
  echo ERROR: Cannot copy zlib DLL to "%DEPLOY_DIR%"
  goto :fail
)

rem  ****************
rem  Copy OpenSSL
rem  ****************
copy /Y "%OPENSSLROOT%\bin\*.dll" "%DEPLOY_DIR%"

if errorlevel 1 (
  echo ERROR: Cannot copy OpenSSL DLL to "%DEPLOY_DIR%"
  goto :fail
)

rem  ****************
rem  Deploy
rem  ****************
pushd "%DEPLOY_DIR%" >nul
if errorlevel 1 (
  echo ERROR: Cannot enter DEPLOY_DIR: "%DEPLOY_DIR%"
  goto :fail
)

echo --- windeployqt (release) ---
"%WINDEPLOYQT%" -release --openssl-root "%OPENSSLROOT%" --skip-plugin-types qmltooling,position,qml,qsqlpsql,qsqlodbc,qsqlmimer "qlog.exe"
if errorlevel 1 (
  popd >nul
  goto :fail
)
popd >nul

if not exist "%BINARYCREATOR%" (
  echo ERROR: binarycreator.exe not found: "%BINARYCREATOR%"
  goto :fail
)

echo --- binarycreator ---
"%BINARYCREATOR%" -f -c "%INSTALLER_CONFIG%" -p "%INSTALLER_PACKAGES%" "%INSTALLER_OUT%"
if errorlevel 1 goto :fail

goto :ok

:all
call :rebuild
if errorlevel 1 goto :fail
call :deploy
if errorlevel 1 goto :fail
goto :ok

rem ============================================================
rem  END
rem ============================================================
:ok
popd >nul
endlocal
exit /b 0

:fail
set "EC=%errorlevel%"
if "%EC%"=="0" set "EC=1"
echo.
echo FAILED with errorlevel %EC%
popd >nul
endlocal
exit /b %EC%
