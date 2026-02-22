@echo off
REM Build script for CavernAudioDriver using VS Developer Command Prompt
cd /d C:\Users\nicol\.openclaw\workspace\workspace\CavernAudioDriver
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"
set VisualStudioVersion=17.0
MSBuild CavernAudioDriver.sln /p:Configuration=Debug /p:Platform=x64 /p:TargetVersion=Windows10
exit /b %ERRORLEVEL%
