@echo off
setlocal
cd /d "%~dp0\..\.."
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0deploy-published.ps1" -WaitForMain
if errorlevel 1 (
  echo.
  echo DEPLOY FAILED
  pause
)
endlocal
