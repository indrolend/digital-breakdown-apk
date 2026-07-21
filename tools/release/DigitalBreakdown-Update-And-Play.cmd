@echo off
setlocal
set "GAME_ROOT=%~dp0"
set "UPDATER=%LOCALAPPDATA%\DigitalBreakdown\updater\get-latest-native.ps1"
if not exist "%UPDATER%" set "UPDATER=%GAME_ROOT%updater\get-latest-native.ps1"
set "FALLBACK=%GAME_ROOT%DigitalBreakdown.exe"

if not exist "%UPDATER%" (
  echo Digital Breakdown updater is missing. Starting the bundled game.
  start "" /D "%GAME_ROOT%" "%FALLBACK%"
  exit /b 0
)

powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%UPDATER%" -Platform Windows -Launch -FallbackExecutable "%FALLBACK%"
if errorlevel 1 (
  echo.
  echo The update check failed. Starting the bundled verified game instead.
  start "" /D "%GAME_ROOT%" "%FALLBACK%"
)
