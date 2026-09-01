@echo off
title MyMP Server (FXServer-style artifacts)
echo =====================================================
echo   MyMP Server  —  like FiveM's FXServer artifacts:
echo   edit server.cfg, then run this file.
echo   Admin panel comes up at http://localhost:40120
echo =====================================================
echo.
where python >nul 2>&1
if errorlevel 1 (
    echo [ERROR] Python 3.10+ not found.
    echo         Install it from https://www.python.org/downloads/
    echo         (tick "Add python.exe to PATH" during install)
    pause
    exit /b 1
)
echo [1/1] Starting MyMP server on 0.0.0.0:30120 ...
echo       (players connect with MyMP-Setup.exe or the browser client)
echo.
python server\main.py --port 30120
echo.
echo Server stopped. Press any key to close.
pause >nul
