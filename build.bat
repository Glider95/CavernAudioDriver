@echo off
chcp 65001 >nul
setlocal EnableDelayedExpansion

echo.
echo  ╔═══════════════════════════════════════════════════════════╗
echo  ║      CavernAudioDriver Build Script                       ║
echo  ║      Windows Virtual Audio Driver for Dolby Atmos         ║
echo  ╚═══════════════════════════════════════════════════════════╝
echo.

:: Check for Visual Studio 2022
set "VS_PATH="
for /f "delims=" %%i in ('"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath 2^>nul') do (
    set "VS_PATH=%%i"
)

if not defined VS_PATH (
    echo  [ERROR] Visual Studio 2022 with C++ tools not found!
    echo.
    echo  Please install Visual Studio 2022 with:
    echo    - Desktop development with C++ workload
    echo    - MSVC v143 build tools
    echo    - Windows 10/11 SDK
    pause
    exit /b 1
)

echo  [INFO] Found Visual Studio at: %VS_PATH%

:: Check for Windows Driver Kit (WDK)
if not exist "C:\Program Files (x86)\Windows Kits\10\" (
    echo.
    echo  [ERROR] Windows Driver Kit (WDK) not found!
    echo.
    echo  Please install WDK 10 from:
    echo  https://docs.microsoft.com/en-us/windows-hardware/drivers/download-the-wdk
    pause
    exit /b 1
)

echo  [INFO] Windows Driver Kit found

:: Setup environment
set "VSCMD_START_DIR=%CD%"
call "%VS_PATH%\VC\Auxiliary\Build\vcvars64.bat" > nul

if errorlevel 1 (
    echo  [ERROR] Failed to initialize build environment
    pause
    exit /b 1
)

echo  [INFO] Build environment initialized

:: Parse arguments
set "CONFIG=Debug"
set "PLATFORM=x64"

if /i "%~1"=="release" set "CONFIG=Release"
if /i "%~1"=="debug" set "CONFIG=Debug"

echo.
echo  [BUILD] Configuration: %CONFIG%
echo  [BUILD] Platform: %PLATFORM%
echo.

:: Create output directories
if not exist "bin\%CONFIG%" mkdir "bin\%CONFIG%"
if not exist "obj\%CONFIG%" mkdir "obj\%CONFIG%"

:: Build
echo  [BUILD] Starting MSBuild...
msbuild CavernAudioDriver.sln /p:Configuration=%CONFIG% /p:Platform=%PLATFORM% /m /verbosity:minimal

if errorlevel 1 (
    echo.
    echo  [ERROR] Build failed!
    echo.
    pause
    exit /b 1
)

echo.
echo  [SUCCESS] Build completed!
echo.
echo  Output:
echo    - Driver: bin\%PLATFORM%\%CONFIG%\CavernAudioDriver.sys
echo    - INF:    bin\%PLATFORM%\%CONFIG%\CavernAudioDriver.inf
echo.

:: Check if we should install (requires admin)
echo  To install the driver (requires admin):
echo    cd bin\%PLATFORM%\%CONFIG%
echo    pnputil /add-driver CavernAudioDriver.inf /install
echo.

pause
