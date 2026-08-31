@echo off
where hud.cmd >nul 2>nul || (
  echo CommandHUD is not installed.
  echo Install the product once with:
  echo npm install --global "git+https://github.com/indrolend/hate.this.meaningless.life.git#convergence/generic-commandhud-20260825"
  pause
  exit /b 1
)
hud desktop --root "%~dp0."
if errorlevel 1 pause
