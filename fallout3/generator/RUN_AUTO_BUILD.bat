@echo off
setlocal
cd /d "%~dp0"

if "%~3"=="" (
  echo Usage:
  echo   RUN_AUTO_BUILD.bat "PhoBoi_FO3_WorldScan.zip" "Fallout1_MAPS.zip" "Fallout2_maps.zip" [output]
  echo.
  echo Optional Tactics DLC is run from Python with --tactics and --profile.
  pause
  exit /b 1
)

set OUT=%~4
if "%OUT%"=="" set OUT=FO3_GENERATED

python build_fallout3.py --scan "%~1" --fo1 "%~2" --fo2 "%~3" --output "%OUT%"
if errorlevel 1 pause
