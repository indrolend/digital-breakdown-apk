@echo off
setlocal
set "DB_VARIANT=%~1"
if "%DB_VARIANT%"=="" set "DB_VARIANT=rabid-animator"
if not exist "%~dp0build\desktop-release\bin\Release\DigitalBreakdown.exe" powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\dbdev.ps1" desktop-build
if errorlevel 1 exit /b %errorlevel%
start "Digital Breakdown - %DB_VARIANT%" "%~dp0build\desktop-release\bin\Release\DigitalBreakdown.exe" --enemy-variant "%DB_VARIANT%"
