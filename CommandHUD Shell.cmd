@echo off
setlocal
title CommandHUD Shell

where node.exe >nul 2>nul
if errorlevel 1 (
  echo CommandHUD needs Node.js, but node.exe was not found.
  echo Install Node.js, reopen this launcher, and try again.
  echo.
  pause
  exit /b 1
)

if not exist "%~dp0tools\hud\cli.mjs" (
  echo CommandHUD could not find its files beside this launcher.
  echo Expected: "%~dp0tools\hud\cli.mjs"
  echo Keep CommandHUD Shell.cmd in the repository root.
  echo.
  pause
  exit /b 1
)

node "%~dp0tools\hud\cli.mjs" shell --root "%~dp0."
set "COMMANDHUD_EXIT=%ERRORLEVEL%"
if not "%COMMANDHUD_EXIT%"=="0" (
  echo.
  echo CommandHUD stopped with exit code %COMMANDHUD_EXIT%.
  pause
)
exit /b %COMMANDHUD_EXIT%
