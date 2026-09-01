@echo off
title My FiveM Launcher
cd /d "%~dp0"
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0launch-fivem.ps1"
