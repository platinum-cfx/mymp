@echo off
title MyMP - Install to GTA V
setlocal EnableDelayedExpansion

echo ============================================
echo   MyMP - Install client into GTA V
echo ============================================
echo.

REM ---- 1. find GTA V ----
set "GTA="
for /f "tokens=2,*" %%A in ('reg query "HKLM\SOFTWARE\WOW6432Node\Valve\Steam" /v InstallPath 2^>nul') do set "STEAM=%%B"
if defined STEAM (
  if exist "!STEAM!\steamapps\common\Grand Theft Auto V\GTA5.exe" set "GTA=!STEAM!\steamapps\common\Grand Theft Auto V"
  if exist "!STEAM!\steamapps\common\Grand Theft Auto V Enhanced\GTA5.exe" set "GTA=!STEAM!\steamapps\common\Grand Theft Auto V Enhanced"
)
if not defined GTA (
  for /f "tokens=2,*" %%A in ('reg query "HKLM\SOFTWARE\WOW6432Node\Rockstar Games\Grand Theft Auto V" /v InstallFolder 2^>nul') do set "GTA=%%B"
)
if not defined GTA (
  echo Could not find GTA V automatically.
  set /p "GTA=Enter the full path of your GTA V folder (where GTA5.exe is): "
)
if not exist "!GTA!\GTA5.exe" (
  echo ERROR: GTA5.exe not found in !GTA!
  pause
  exit /b 1
)
echo GTA V found: !GTA!
echo.

REM ---- 2. install ASI loader + client ----
copy /y "%~dp0dinput8.dll" "!GTA!\dinput8.dll" >nul
copy /y "%~dp0MyMP.asi" "!GTA!\MyMP.asi" >nul
if not exist "!GTA!\mymp.ini" copy /y "%~dp0mymp.ini" "!GTA!\mymp.ini" >nul
echo Installed: dinput8.dll (ASI loader), MyMP.asi (client), mymp.ini (config)
echo.

REM ---- 3. server address ----
set /p "HOST=MyMP server IP (Enter = 127.0.0.1 for same PC): "
if not defined HOST set "HOST=127.0.0.1"
powershell -NoProfile -Command "(Get-Content '!GTA!\mymp.ini') -replace '(?m)^host=.*$', 'host=%HOST%' | Set-Content '!GTA!\mymp.ini'"
echo Server set to: %HOST%
echo.

REM ---- 4. launch ----
echo Launching GTA V. In-game your vehicle should spawn and connect.
start "" "!GTA!\GTA5.exe"
echo.
echo If nothing happens in-game, open !GTA!\mymp.log and check for errors.
echo (See TESTING.md for what to look for.)
pause
