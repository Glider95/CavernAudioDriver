@echo off
REM Build script for CavernAudioDriver (SysVAD-based)
cd /d C:\Users\nicol\.openclaw\workspace\workspace\CavernAudioDriver\CavernSysvad

call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"
set VisualStudioVersion=17.0

REM Build the driver
echo Building CavernAudioDriver...
MSBuild CavernAudioDriver.vcxproj /p:Configuration=Debug /p:Platform=x64 /p:SignMode=Off

if %ERRORLEVEL% NEQ 0 (
    echo Build failed!
    exit /b %ERRORLEVEL%
)

echo Build successful!
echo Output: bin\Debug\CavernAudioDriver.sys
