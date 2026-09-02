# ============================================================
#  build.ps1 — build the MyMP GTA V client (MyMP.asi)
#
#  Requirements: Windows 10/11 + "Build Tools for Visual Studio
#  2022" (free, includes cl.exe) — or full Visual Studio.
#  If Visual Studio is installed, this script finds vcvars64.bat
#  automatically. Run:  powershell -File build.ps1
# ============================================================
$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path

# --- locate vcvars64.bat ---
$vcvars = $null
$candidates = @(
    "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat",
    "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat",
    "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat",
    "${env:ProgramFiles}\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat",
    "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
)
foreach ($c in $candidates) { if (Test-Path $c) { $vcvars = $c; break } }
if (-not $vcvars) {
    Write-Host "Could not find vcvars64.bat." -ForegroundColor Red
    Write-Host "Install free 'Build Tools for Visual Studio 2022' from:" -ForegroundColor Yellow
    Write-Host "  https://visualstudio.microsoft.com/downloads/#build-tools-for-visual-studio-2022"
    Write-Host "(select 'Desktop development with C++' workload) and re-run this script."
    exit 1
}

$src = Join-Path $here "src"
$out = Join-Path $here "MyMP.asi"
$lua = Join-Path $src "lua"
$glmFlags = "/DGLM_ENABLE_EXPERIMENTAL /DGLM_FORCE_INTRINSICS /DGLM_FORCE_INLINE /DGLM_FORCE_Z_UP " +
            "/DLUA_GLM_INCLUDE_ALL /DLUA_GLM_ALIASES /DLUA_GLM_GEOM_EXTENSIONS /DLUA_GLM_RECYCLE"

# --- 1. Lua VM (the real FiveM Lua: citizenfx/lua + LuaGLM), compiled as C++ ---
$luaObjs = @()
foreach ($c in Get-ChildItem -Path $lua -Filter "*.c") {
    $o = Join-Path $lua ($c.BaseName + ".obj")
    $lcmd = "`"$vcvars`" && cl /nologo /O2 /EHsc /MD /W3 /TP /c $glmFlags /DLUA_INCLUDE_LIBGLM " +
            "/I`"$lua`" /I`"$lua\libs`" /I`"$lua\libs\glm-binding`" `"$($c.FullName)`" /Fo`"$o`""
    cmd /c $lcmd | Out-Null
    if (-not (Test-Path $o)) { Write-Host "Lua compile FAILED: $($c.Name)" -ForegroundColor Red; exit 1 }
    $luaObjs += "`"$o`""
}
foreach ($cpp in @("lglm.cpp", "libs\glm-binding\lglmlib.cpp")) {
    $full = Join-Path $lua $cpp
    $o = Join-Path $lua ([IO.Path]::GetFileNameWithoutExtension($cpp) + ".obj")
    $lcmd = "`"$vcvars`" && cl /nologo /O2 /EHsc /MD /W3 $glmFlags /DLUA_INCLUDE_LIBGLM " +
            "/I`"$lua`" /I`"$lua\libs`" /I`"$lua\libs\glm-binding`" `"$full`" /Fo`"$o`""
    cmd /c $lcmd | Out-Null
    if (-not (Test-Path $o)) { Write-Host "Lua compile FAILED: $cpp" -ForegroundColor Red; exit 1 }
    $luaObjs += "`"$o`""
}

# --- 2. Client + scripting runtime (Lua includes are C++-linked, no extern "C") ---
$cmd = "`"$vcvars`" && cl /nologo /O2 /EHsc /MD /W3 /D_CRT_SECURE_NO_WARNINGS /DLUA_INCLUDE_LIBGLM " +
       "/I`"$src`" " +
       "`"$src\net.cpp`" `"$src\scriptthread.cpp`" `"$src\client.cpp`" " +
       "`"$src\script_rt.cpp`" `"$src\http_get.cpp`" `"$src\json.cpp`" `"$src\lua_native_bindings.cpp`" " +
       ($luaObjs -join " ") +
       " /Fe:`"$out`" /link ws2_32.lib psapi.lib"
Write-Host "Compiling MyMP.asi ..." -ForegroundColor Cyan
cmd /c $cmd
if (-not (Test-Path $out)) { Write-Host "Build FAILED." -ForegroundColor Red; exit 1 }
$size = (Get-Item $out).Length / 1KB
Write-Host "OK -> $out ($([math]::Round($size,1)) KB)" -ForegroundColor Green
Write-Host "Next: run 'Install & Launch MyMP.bat' in the launcher folder,"
Write-Host "or copy MyMP.asi into your GTA V folder (needs an ASI loader like Ultimate ASI Loader)."
