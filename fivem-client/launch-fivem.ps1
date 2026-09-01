# ============================================================
#  My FiveM Launcher  (custom launcher for your own FiveM)
#  - Finds your GTA V installation (Steam / Epic / Rockstar)
#  - Finds or installs the FiveM client next to it
#  - Launches FiveM
#  Works on Windows 10 / 11. No admin rights needed.
# ============================================================

$ErrorActionPreference = "Stop"

function Find-GTAV {
    # 1) Steam registry
    try {
        $steamPath = (Get-ItemProperty -Path "HKLM:\SOFTWARE\WOW6432Node\Valve\Steam" -ErrorAction Stop).InstallPath
        $candidates = @(
            "$steamPath\steamapps\common\Grand Theft Auto V",
            "$steamPath\steamapps\common\Grand Theft Auto V Enhanced"
        )
        foreach ($c in $candidates) {
            if (Test-Path "$c\GTA5.exe") { return $c }
        }
    } catch {}

    # 2) Rockstar Games Launcher registry
    try {
        $rk = (Get-ItemProperty -Path "HKLM:\SOFTWARE\WOW6432Node\Rockstar Games\Grand Theft Auto V" -ErrorAction Stop).InstallFolder
        if ($rk -and (Test-Path "$rk\GTA5.exe")) { return $rk }
    } catch {}

    # 3) Common default locations
    $common = @(
        "$env:ProgramFiles\Rockstar Games\Grand Theft Auto V",
        "${env:ProgramFiles(x86)}\Rockstar Games\Grand Theft Auto V",
        "C:\Games\Grand Theft Auto V",
        "D:\Games\Grand Theft Auto V",
        "D:\SteamLibrary\steamapps\common\Grand Theft Auto V",
        "E:\SteamLibrary\steamapps\common\Grand Theft Auto V"
    )
    foreach ($c in $common) {
        if (Test-Path "$c\GTA5.exe") { return $c }
    }
    return $null
}

Write-Host ""
Write-Host "==============================" -ForegroundColor Cyan
Write-Host "   MY FIVEM LAUNCHER" -ForegroundColor Cyan
Write-Host "==============================" -ForegroundColor Cyan

$gtaPath = Find-GTAV
if (-not $gtaPath) {
    Write-Host ""
    Write-Host "Could not find GTA V automatically." -ForegroundColor Yellow
    $gtaPath = Read-Host "Enter the full path of your GTA V folder (where GTA5.exe is)"
    if (-not (Test-Path "$gtaPath\GTA5.exe")) {
        Write-Host "That folder does not contain GTA5.exe." -ForegroundColor Red
        Read-Host "Press Enter to exit"
        exit 1
    }
}

Write-Host ""
Write-Host "GTA V found at: $gtaPath" -ForegroundColor Green

$fivemApp = Join-Path $gtaPath "FiveM.app"
$fivemExe = Join-Path $fivemApp "FiveM.exe"

if (-not (Test-Path $fivemExe)) {
    Write-Host "FiveM is not installed next to your GTA V yet." -ForegroundColor Yellow

    # Look for the installer next to this script, otherwise download it
    $installer = Join-Path $PSScriptRoot "FiveM_Installer.exe"
    if (-not (Test-Path $installer)) {
        Write-Host "Downloading the official FiveM installer from cfx.re..." -ForegroundColor Cyan
        $installer = Join-Path $env:TEMP "FiveM_Installer.exe"
        Invoke-WebRequest -Uri "https://runtime.fivem.net/client/FiveM.exe" -OutFile $installer
    }

    Write-Host "Starting the FiveM installer. It will place FiveM.app next to your GTA V." -ForegroundColor Cyan
    Start-Process -FilePath $installer -WorkingDirectory $PSScriptRoot
    Write-Host ""
    Write-Host "When the installer finishes, run this launcher again." -ForegroundColor Green
    Read-Host "Press Enter to exit"
    exit 0
}

Write-Host "Starting FiveM... Have fun!" -ForegroundColor Green
Start-Process -FilePath $fivemExe
