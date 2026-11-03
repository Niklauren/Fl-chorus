@echo off
rem Builds FL Chorus (VST3, 64-bit) and the Windows installer:
rem   dist\FL-Chorus-1.0.0-win-setup.exe
rem Needs: Visual Studio 2022 (Desktop development with C++), CMake, Git, Inno Setup 6.3+
setlocal
cd /d "%~dp0\..\.."

cmake -B build-release -G "Visual Studio 17 2022" -A x64 -DFLC_COPY_AFTER_BUILD=OFF || goto :error
cmake --build build-release --config Release --parallel || goto :error

set ISCC="%ProgramFiles(x86)%\Inno Setup 6\ISCC.exe"
if not exist %ISCC% set ISCC="%ProgramFiles%\Inno Setup 6\ISCC.exe"
%ISCC% installer\windows\FLChorus.iss || goto :error

echo.
echo Installer ready in the dist folder.
exit /b 0

:error
echo Build failed.
exit /b 1
