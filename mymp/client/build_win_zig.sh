#!/usr/bin/env bash
# ============================================================
#  build_win_zig.sh — cross-build the MyMP GTA V client from
#  Linux/macOS (no Windows needed): produces MyMP.asi + the
#  FiveM-style launcher (MyMP.exe, self-contained with the
#  ASI + dinput8 loader embedded) + MyMP-Setup.exe.
#
#  Uses zig cc (bundled clang + mingw headers + lld) and a
#  static Opus 1.5.2 for voice — same sources, same defines as
#  build.ps1 (the MSVC path), so either build is interchangeable.
#
#  Usage:
#    ZIG_VERSION=0.14.1 OPUS_VERSION=1.5.2 ./build_win_zig.sh
#    # then: cp MyMP.asi MyMP-Launcher.exe MyMP-Setup.exe MyMP.exe ...
# ============================================================
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
SRC="$HERE/src"
ZIG_VERSION="${ZIG_VERSION:-0.14.1}"
OPUS_VERSION="${OPUS_VERSION:-1.5.2}"
WORK="${WORK:-/tmp/mymp-winbuild}"

mkdir -p "$WORK"
cd "$WORK"

# ---------- toolchain ----------
if ! command -v zig >/dev/null 2>&1; then
  if [ ! -x "zig-x86_64-linux-$ZIG_VERSION/zig" ]; then
    echo "== downloading zig $ZIG_VERSION =="
    curl -sL -o zig.tar.xz "https://ziglang.org/download/$ZIG_VERSION/zig-x86_64-linux-$ZIG_VERSION.tar.xz"
    tar xf zig.tar.xz
  fi
  export PATH="$WORK/zig-x86_64-linux-$ZIG_VERSION:$PATH"
fi
ZIG="$(command -v zig)"

# ---------- opus (static, for in-game voice) ----------
if [ ! -f "$WORK/libopus_win.a" ]; then
  echo "== building opus $OPUS_VERSION (win x64 static) =="
  curl -sL -o opus.tar.gz "https://github.com/xiph/opus/releases/download/v$OPUS_VERSION/opus-$OPUS_VERSION.tar.gz"
  tar xzf opus.tar.gz
  cd "opus-$OPUS_VERSION"
  cat > config.h <<'EOF'
#define PACKAGE_VERSION "1.5.2"
#define OPUS_BUILD 1
#define HAVE_LRINTF 1
#define HAVE_LRINT 1
#define OPUS_HAVE_STDINT_H 1
EOF
  OPUS_FILES="$(sed -n 's/^OPUS_SOURCES\(_FLOAT\)\? = //p' opus_sources.mk | tr -d '\\' | tr ' ' '\n' | grep '\.c$' \
               ; sed -n 's/^CELT_SOURCES = //p' celt_sources.mk | tr -d '\\' | tr ' ' '\n' | grep '\.c$' \
               ; sed -n 's/^SILK_SOURCES\(_FLOAT\)\? = //p' silk_sources.mk | tr -d '\\' | tr ' ' '\n' | grep '\.c$')"
  OPUS_FILES="$(echo "$OPUS_FILES" | sort -u)"
  mkdir -p obj
  for f in $OPUS_FILES; do
    $ZIG cc -target x86_64-windows-gnu -O2 -DOPUS_BUILD -DHAVE_CONFIG_H -DUSE_ALLOCA \
        -I. -Iinclude -Icelt -Isilk -Isilk/float -Isrc -c "$f" -o "obj/$(echo "$f" | tr '/' '_').o"
  done
  $ZIG ar rcs "$WORK/libopus_win.a" obj/*.o
  cd "$WORK"
fi

# ---------- flags (same as build.ps1) ----------
CFLAGS="-target x86_64-windows-gnu -O2 -D_CRT_SECURE_NO_WARNINGS -DLUA_INCLUDE_LIBGLM \
-DGLM_ENABLE_EXPERIMENTAL -DGLM_FORCE_INTRINSICS -DGLM_FORCE_INLINE -DGLM_FORCE_Z_UP \
-DLUA_GLM_INCLUDE_ALL -DLUA_GLM_ALIASES -DLUA_GLM_GEOM_EXTENSIONS -DLUA_GLM_RECYCLE"
INC="-I$SRC -I$SRC/lua -I$SRC/lua/libs -I$SRC/lua/libs/glm-binding -I$WORK/opus-$OPUS_VERSION/include"

echo "== compiling Lua VM (Cfx Lua 5.4.4 + LuaGLM, as C++) =="
rm -rf obj && mkdir -p obj/lua
for f in "$SRC"/lua/*.c; do
  $ZIG c++ $CFLAGS $INC -x c++ -c "$f" -o "obj/lua/$(basename "${f%.c}").o"
done
$ZIG c++ $CFLAGS $INC -c "$SRC/lua/lglm.cpp" -o obj/lua/lglm.o
$ZIG c++ $CFLAGS $INC -c "$SRC/lua/libs/glm-binding/lglmlib.cpp" -o obj/lua/lglmlib.o

echo "== compiling client =="
for f in net.cpp scriptthread.cpp client.cpp script_rt.cpp http_get.cpp json.cpp \
         lua_native_bindings.cpp voice_audio.cpp voice_core.cpp; do
  $ZIG c++ $CFLAGS $INC -c "$SRC/$f" -o "obj/${f%.cpp}.o"
done

echo "== linking MyMP.asi =="
$ZIG c++ -target x86_64-windows-gnu -shared -O2 -o "$WORK/MyMP.asi" \
    obj/*.o obj/lua/*.o "$WORK/libopus_win.a" \
    -lws2_32 -lpsapi -lole32 -lwinmm -ladvapi32

echo "== building launcher + installer =="
$ZIG cc -target x86_64-windows-gnu -O2 -Wl,--subsystem,windows \
    -lwinhttp -lws2_32 -ladvapi32 -lgdi32 -luser32 \
    "$HERE/launcher/mymp_launcher.c" -o "$WORK/MyMP-Launcher.exe"
$ZIG cc -target x86_64-windows-gnu -O2 -ladvapi32 \
    "$HERE/installer/mymp_setup.c" -o "$WORK/MyMP-Setup.exe"

echo "== assembling self-contained MyMP.exe (payload: asi + dinput8 loader) =="
DINPUT8="${DINPUT8:-$WORK/dinput8.dll}"
if [ ! -f "$DINPUT8" ]; then
  echo "  !! dinput8.dll (Ultimate ASI Loader) not found — put it at $DINPUT8"
  echo "     or run the Windows build.asm step; skipping payload assembly."
else
  python3 - "$WORK/MyMP-Launcher.exe" "$WORK/MyMP.asi" "$DINPUT8" "$WORK/MyMP.exe" <<'PYEOF'
import struct, sys
launcher, asi, dll = [open(p, "rb").read() for p in sys.argv[1:4]]
asioff, dlloff = len(launcher), len(launcher) + len(asi)
hdr = struct.pack("<8sQQQQ", b"MYMPXSE1", asioff, len(asi), dlloff, len(dll))
open(sys.argv[4], "wb").write(launcher + asi + dll + hdr)
print(f"  MyMP.exe = {len(launcher)}+{len(asi)}+{len(dll)}+40 bytes")
PYEOF
fi

echo
echo "== done: $WORK/MyMP.asi  $WORK/MyMP-Launcher.exe  $WORK/MyMP-Setup.exe  $WORK/MyMP.exe =="
