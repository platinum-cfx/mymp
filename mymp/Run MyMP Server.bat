@echo off
title MyMP Server
cd /d "%~dp0"
echo Checking Python...
python --version >nul 2>&1
if errorlevel 1 (
  echo Python is required - install it from https://python.org (tick "Add to PATH")
  pause
  exit /b 1
)
echo Starting MyMP server...
start "" http://localhost:30120
python server\main.py
pause
