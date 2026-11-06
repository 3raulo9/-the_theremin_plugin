@echo off
rem ---------------------------------------------------------------------------
rem  Copies the built plugin into the standard VST3 folder so FL Studio (and
rem  every other DAW) finds it. Asks for administrator rights (UAC) because
rem  the folder lives under Program Files.
rem ---------------------------------------------------------------------------
setlocal

set "SRC=%~dp0build\AetherTheremin_artefacts\Release\VST3\Aether Theremin.vst3"
set "DST=%CommonProgramFiles%\VST3\Aether Theremin.vst3"

if not exist "%SRC%" (
    echo The plugin has not been built yet - run build.bat first.
    pause
    exit /b 1
)

net session >nul 2>&1
if %errorlevel% neq 0 (
    echo Requesting administrator rights...
    powershell -NoProfile -Command "Start-Process -FilePath '%~f0' -Verb RunAs"
    exit /b
)

xcopy /E /I /Y /Q "%SRC%" "%DST%\" >nul || (
    echo Copy failed. Is FL Studio running with the plugin loaded? Close it and try again.
    pause
    exit /b 1
)

echo Installed to "%DST%"
echo In FL Studio: Options ^> Manage plugins ^> Find installed plugins.
pause
endlocal
