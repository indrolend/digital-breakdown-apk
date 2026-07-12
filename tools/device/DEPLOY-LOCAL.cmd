@echo off
setlocal
cd /d "%~dp0\..\.."
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0deploy-local.ps1"
if errorlevel 1 (
  echo.
  echo DEPLOY FAILED
  pause
)
endlocal
