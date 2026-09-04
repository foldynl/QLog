@echo off
setlocal EnableExtensions EnableDelayedExpansion

rem ============================================================
rem  Build + Run all unit tests for QLog (MSVC + Qt)
rem  Usage:
rem    %~nx0              -> build (release) + run
rem    %~nx0 all          -> build (release) + run
rem    %~nx0 build        -> build only (release)
rem    %~nx0 run          -> run existing binaries
rem    %~nx0 clean        -> clean build directory
rem    %~nx0 rebuild      -> clean + build + run
rem ============================================================
rem  Notes:
rem  - This script is executed in cmd.exe on Windows.
rem  - It expects MSVC environment init batch (VS_VCVARS) and Qt qmake.
rem  - QTKEYCHAIN/HAMLIB/ZLIB paths are passed through like in make.bat.
rem ============================================================

set "SCRIPT_DIR=%~dp0"
if "%SCRIPT_DIR:~-1%"=="\" set "SCRIPT_DIR=%SCRIPT_DIR:~0,-1%"
for %%I in ("%SCRIPT_DIR%\..\..") do set "PROJECT_BASE=%%~fI"
for %%I in ("%PROJECT_BASE%\..") do set "DEVROOT=%%~fI"

rem === CONFIGURATION (keep in sync with make.bat) ===

rem -- VC Compiler Settings
set "VS_VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat"
set "VS_ARCH=x86_amd64"

rem -- QT Settings
set "QT_BASE=C:\Qt\6.11.1\msvc2022_64"
set "JOM=C:\Qt\Tools\QtCreator\bin\jom\jom.exe"

rem -- Project Settings
set "PRO=%PROJECT_BASE%\tests\tests.pro"
set "BUILDROOT=%PROJECT_BASE%\build\tests"

rem -- Libs Settings
set "VCPKG_PACKAGES=%DEVROOT%\vcpkg\packages"
set "VCPKG_QTKEYCHAIN_PKG=qtkeychain-qt6_x64-windows"
set "VCPKG_PTHREAD_PKG=pthreads_x64-windows"
set "VCPKG_ZLIB_PKG=zlib_x64-windows"

rem -- Hamlib Settings
set "HAMLIBVERSION_MAJOR=4"
set "HAMLIBVERSION_MINOR=7"
set "HAMLIBVERSION_PATCH=2"

rem -- OpenSSL (needed at runtime for some configurations)
set "OPENSSLROOT=%DEVROOT%\openssl-3.0\x64"

rem === END OF CONFIGURATION ===

set "QMAKE=%QT_BASE%\bin\qmake.exe"

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

set "ACTION=%~1"
if /I "%ACTION%"=="" set "ACTION=all"

if not exist "%PRO%" (
  echo ERROR: tests project not found: "%PRO%"
  goto :fail
)

if not exist "%BUILDROOT%" mkdir "%BUILDROOT%"
pushd "%BUILDROOT%" >nul

echo === Initializing MSVC environment (%VS_ARCH%) ===
call "%VS_VCVARS%" %VS_ARCH%
if errorlevel 1 goto :fail

if exist "%JOM%" (
  set "MAKE=%JOM%"
) else (
  set "MAKE=nmake"
)

if /I "%ACTION%"=="clean"   goto :action_clean
if /I "%ACTION%"=="build"   goto :action_build
if /I "%ACTION%"=="run"     goto :action_run
if /I "%ACTION%"=="rebuild" goto :action_rebuild
if /I "%ACTION%"=="all"     goto :action_all

echo Unknown action: %ACTION%
echo Usage: %~nx0 [all^|build^|run^|clean^|rebuild]
goto :fail

:qmake
echo === Running qmake (tests, release) ===
"%QMAKE%" "%PRO%" -spec win32-msvc ^
  "CONFIG+=release" ^
  "CONFIG-=debug_and_release" ^
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
exit /b 0

:doBuild
call :qmake
if errorlevel 1 exit /b 11

echo === Building tests ===
"%MAKE%"
if errorlevel 1 exit /b 12
exit /b 0

:doRun
echo === Running tests ===
set "PATH=%QT_BASE%\bin;%QTKEYCHAINBINPATH%;%HAMLIBBINPATH%;%ZLIBBINPATH%;%OPENSSLROOT%\bin;%PATH%"

set "FAILED=0"
set "COUNT=0"
for /r "%BUILDROOT%" %%F in (tst_*.exe) do (
  set /a COUNT+=1
  echo --- %%~nxF ---
  "%%F"
  if errorlevel 1 (
    echo FAILED: %%~nxF
    set "FAILED=1"
    goto :run_done
  )
)

:run_done
if "%COUNT%"=="0" (
  echo ERROR: No test executables found under "%BUILDROOT%".
  exit /b 21
)
if "%FAILED%"=="1" exit /b 20
exit /b 0

:action_clean
echo === Cleaning build directory ===
if exist "%BUILDROOT%\Makefile" (
  "%MAKE%" clean
)
popd >nul
rmdir /S /Q "%BUILDROOT%" >nul 2>&1
endlocal
exit /b 0

:action_build
call :doBuild
if errorlevel 1 goto :fail
goto :ok

:action_run
call :doRun
if errorlevel 1 goto :fail
goto :ok

:action_rebuild
echo === Rebuild ===
if exist "%BUILDROOT%\Makefile" (
  "%MAKE%" clean
)
call :doBuild
if errorlevel 1 goto :fail
call :doRun
if errorlevel 1 goto :fail
goto :ok

:action_all
call :doBuild
if errorlevel 1 goto :fail
call :doRun
if errorlevel 1 goto :fail
goto :ok

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
