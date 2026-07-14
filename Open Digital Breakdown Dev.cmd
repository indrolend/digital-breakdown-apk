@echo off
setlocal
cd /d "%~dp0"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\dbdev.ps1" ui
if errorlevel 1 (
  echo.
  echo Digital Breakdown Dev failed to start.
  pause
)
