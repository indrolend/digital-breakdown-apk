@echo off
setlocal
title CommandHUD Shell

where hud.cmd >nul 2>nul
if errorlevel 1 (
  echo CommandHUD is not installed.
  echo Install the product once with:
  echo npm install --global "git+https://github.com/indrolend/hate.this.meaningless.life.git#convergence/generic-commandhud-20260825"
  echo.
  pause
  exit /b 1
)

hud shell --root "%~dp0." %*
set "COMMANDHUD_EXIT=%ERRORLEVEL%"
if not "%COMMANDHUD_EXIT%"=="0" (
  echo.
  echo CommandHUD stopped with exit code %COMMANDHUD_EXIT%.
  pause
)
exit /b %COMMANDHUD_EXIT%
