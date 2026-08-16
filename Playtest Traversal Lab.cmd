@echo off
setlocal
cd /d "%~dp0"
set "LAB_EXE=%~dp0build\gameplay-checks\bin\Release\DigitalBreakdown.exe"
if not exist "%~dp0build\gameplay-checks\CMakeCache.txt" (
  cmake -S native-desktop -B build\gameplay-checks -DCMAKE_BUILD_TYPE=Release
  if errorlevel 1 goto :failed
)
echo Preparing the latest traversal playtest...
cmake --build build\gameplay-checks --config Release --target DigitalBreakdown
if errorlevel 1 goto :failed
start "Data Traversal Lab" /D "%~dp0build\gameplay-checks\bin\Release" "%LAB_EXE%" --traversal-lab
exit /b 0
:failed
echo.
echo The traversal playtest build could not be prepared.
pause
exit /b 1
