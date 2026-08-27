@echo off
cd /d "%~dp0"
python fo3_classic_forge.py --gui
if errorlevel 1 pause
