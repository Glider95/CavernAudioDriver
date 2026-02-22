@echo off
REM Build script for CavernSimple (SysVAD-based with pipe forwarding)
cd /d C:\Users\nicol\.openclaw\workspace\workspace\CavernAudioDriver\CavernSimple

call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"
set VisualStudioVersion=17.0

REM Build the driver
echo Building CavernSimple...
MSBuild CavernSimple.vcxproj /p:Configuration=Debug /p:Platform=x64 /p:SignMode=Off

if %ERRORLEVEL% NEQ 0 (
    echo Build failed!
    exit /b %ERRORLEVEL%
)

echo Build successful!
echo Output: Debug\CavernSimple.sys
