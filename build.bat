@echo off
rem ---------------------------------------------------------------------------
rem  Builds Aether Theremin (VST3 + Standalone) on Windows.
rem  Requirements: Visual Studio 2022 or newer (or the Build Tools) with the
rem  "Desktop development with C++" workload. CMake and Ninja ship with it.
rem
rem  Usage:  build.bat            (downloads JUCE automatically)
rem          build.bat -DTHEREMIN_JUCE_PATH=C:\path\to\JUCE
rem ---------------------------------------------------------------------------
setlocal

set "ROOT=%~dp0"
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"

if not exist "%VSWHERE%" (
    echo Could not find vswhere.exe - please install Visual Studio with C++ tools.
    exit /b 1
)

for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSDIR=%%i"

if not defined VSDIR (
    echo Visual Studio with the C++ x64 tools was not found.
    exit /b 1
)

call "%VSDIR%\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1

cmake -S "%ROOT%." -B "%ROOT%build" -G Ninja -DCMAKE_BUILD_TYPE=Release %* || exit /b 1
cmake --build "%ROOT%build" || exit /b 1

echo.
echo Done. Your plugin is here:
echo   "%ROOT%build\AetherTheremin_artefacts\Release\VST3\Aether Theremin.vst3"
echo Copy that folder to "C:\Program Files\Common Files\VST3" and rescan plugins in FL Studio.
endlocal
