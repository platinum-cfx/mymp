# ============================================================
#  setup.ps1 — MyMP launcher for GTA V (Windows)
#  Finds your GTA V, installs the ASI loader + MyMP.asi,
#  configures the server address, and launches the game.
#  Run via:  "Install & Launch MyMP.bat"
# ============================================================
$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$client = Join-Path $here "..\client"
$asi = Join-Path $client "MyMP.asi"
$ini = Join-Path $client "mymp.ini.example"

Write-Host ""
Write-Host "==============================" -ForegroundColor Cyan
Write-Host "   MYMP — GTA V LAUNCHER" -ForegroundColor Cyan
Write-Host "==============================" -ForegroundColor Cyan

# --- 1. find GTA V ---
function Find-GTAV {
    try {
        $steam = (Get-ItemProperty "HKLM:\SOFTWARE\WOW6432Node\Valve\Steam" -ErrorAction Stop).InstallPath
        foreach ($sub in @("steamapps\common\Grand Theft Auto V", "steamapps\common\Grand Theft Auto V Enhanced")) {
            if (Test-Path (Join-Path $steam $sub)) { return (Join-Path $steam $sub) }
        }
    } catch {}
    try {
        $rk = (Get-ItemProperty "HKLM:\SOFTWARE\WOW6432Node\Rockstar Games\Grand Theft Auto V" -ErrorAction Stop).InstallFolder
        if ($rk) { return $rk }
    } catch {}
    foreach ($d in @("C:\Program Files\Rockstar Games\Grand Theft Auto V",
                     "C:\Games\Grand Theft Auto V", "D:\Games\Grand Theft Auto V")) {
        if (Test-Path $d) { return $d }
    }
    return $null
}

$gta = Find-GTAV
if (-not $gta -or -not (Test-Path (Join-Path $gta "GTA5.exe"))) {
    Write-Host "Could not find GTA V automatically." -ForegroundColor Yellow
    $gta = Read-Host "Enter the full path of your GTA V folder (with GTA5.exe)"
    if (-not (Test-Path (Join-Path $gta "GTA5.exe"))) { Write-Host "No GTA5.exe there." -ForegroundColor Red; exit 1 }
}
Write-Host "GTA V: $gta" -ForegroundColor Green

# --- 2. ASI loader (needed to inject MyMP.asi) ---
$loader = Join-Path $gta "dinput8.dll"
$bundledLoader = Join-Path (Join-Path $here "..\release") "dinput8.dll"
if (-not (Test-Path $loader)) {
    if (Test-Path $bundledLoader) {
        Copy-Item $bundledLoader $loader
        Write-Host "Installed ASI loader (bundled, MIT Ultimate ASI Loader)." -ForegroundColor Green
    } else {
        Write-Host "No ASI loader found. Downloading Ultimate ASI Loader (open source, MIT)..." -ForegroundColor Cyan
        try {
            $rel = Invoke-RestMethod "https://api.github.com/repos/ThirteenAG/Ultimate-ASI-Loader/releases/latest"
            $asset = $rel.assets | Where-Object { $_.name -eq "dinput8.dll" } | Select-Object -First 1
            if ($asset) {
                Invoke-WebRequest -Uri $asset.browser_download_url -OutFile $loader
                Write-Host "Installed $loader" -ForegroundColor Green
            } else { throw "asset not found" }
        } catch {
            Write-Host "Auto-download failed. Please grab dinput8.dll manually from" -ForegroundColor Yellow
            Write-Host "  https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases" -ForegroundColor Yellow
            Write-Host "and put it in $gta" -ForegroundColor Yellow
        }
    }
} else {
    Write-Host "ASI loader present." -ForegroundColor Green
}

# --- 3. use the prebuilt client (no compiler needed) ---
$asi = Join-Path $client "MyMP.asi"
if (-not (Test-Path $asi)) {
    $prebuilt = Join-Path (Join-Path $here "..\release") "MyMP.asi"
    if (Test-Path $prebuilt) { $asi = $prebuilt }
}
if (-not (Test-Path $asi)) {
    Write-Host "MyMP.asi not found — nothing to install." -ForegroundColor Red
    exit 1
}

# --- 4. install client + config ---
Copy-Item $asi $gta -Force
$targetIni = Join-Path $gta "mymp.ini"
if (-not (Test-Path $targetIni)) { Copy-Item $ini $targetIni }

$hostname = Read-Host "MyMP server IP (Enter = 127.0.0.1)"
if (-not $hostname) { $hostname = "127.0.0.1" }
$content = Get-Content $targetIni -Raw
$content = $content -replace "(?m)^host=.*$", "host=$hostname"
Set-Content $targetIni $content
Write-Host "Client installed. Server: $hostname" -ForegroundColor Green

# --- 5. launch ---
Write-Host "Launching GTA V..." -ForegroundColor Green
Start-Process (Join-Path $gta "GTA5.exe")
Write-Host ""
Write-Host "In game: your vehicle spawns automatically and syncs with the server." -ForegroundColor Cyan
Write-Host "Chat shows as help text in the top-left. Log: $gta\mymp.log"
