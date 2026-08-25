@echo off
node "%~dp0tools\hud\cli.mjs" desktop --root "%~dp0."
if errorlevel 1 pause
