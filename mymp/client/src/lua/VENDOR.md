# Vendored Lua runtime — provenance & license

Everything in `client/src/lua/` is the **real FiveM Lua runtime**, vendored
for MyMP's client-side scripting, not a clean-room reimplementation.

## Sources (all MIT-licensed)

| Path | Source | Version |
|---|---|---|
| `l*.c`, `l*.h`, `lua.hpp`, `luaconf.h` | https://github.com/citizenfx/lua (branch `luaglm-dev/cfx`) | Lua 5.4.4 + Cfx extensions (joaat, vec types in base lib) |
| `lglm.cpp`, `lglm.hpp`, `lglm_core.h`, `lglm_string.hpp`, `lgrit_lib.h` | same repo | LuaGLM core (vector/quaternion/matrix userdata) |
| `libs/glm-binding/` | same repo | LuaGLM API binding (`luaopen_glm`, geometry ext) |
| `libs/glm/` | https://github.com/g-truc/glm at commit `84f2045a79a4aa2454801a98e2de0401bd9c8aee` | GLM — the exact submodule pin used by citizenfx/fivem @ `eab7a55b8f98149ac76af234107e2952c13d4cbb` (2022-12-31) |

The Cfx Lua fork is MIT (Lua.org copyright + Cfx modifications), GLM is MIT
(g-truc). The 2022 FiveM build's own LICENSE lists `vendor/*.lua` and the
scripting components under its LGPLv2 option; the Lua VM itself is the MIT
upstream with Cfx patches.

## How it's built (like FiveM: Lua compiled as C++)

    cd client/src/lua
    g++ -O1 -c -x c++ -std=c++17 \
        -DLUA_INCLUDE_LIBGLM \
        -DGLM_ENABLE_EXPERIMENTAL -DGLM_FORCE_INTRINSICS \
        -DGLM_FORCE_INLINE -DGLM_FORCE_Z_UP \
        -DLUA_GLM_INCLUDE_ALL -DLUA_GLM_ALIASES \
        -DLUA_GLM_GEOM_EXTENSIONS -DLUA_GLM_RECYCLE \
        -I. -Ilibs -Ilibs/glm-binding \
        l*.c lglm.cpp libs/glm-binding/lglmlib.cpp
    ar rcs liblua-cfx.a *.o

Consumers (`script_rt.cpp`, `lua_native_bindings.cpp`) include the Lua
headers **without** `extern "C"` (the VM is C++-linked, so symbols are C++).
Compile them with `-DLUA_INCLUDE_LIBGLM` so the glm library is opened and the
vec3/quat/mat4 metatables install (constructors are globals from the base
library itself, exactly as in FiveM).

## What scripts get (FiveM parity)

- `vec3`, `vec4`, `ivec*`, `bvec*`, `quat`, `mat2`–`mat4` constructors as globals
- `:length()`, `:cross()`, `:dot()`, `+`, `-`, `*`, `==`, component access
- `glm.*` full API table, geometry extension (`aabb`, `ray`, `plane`, ...)
- `joaat` (from base lib, Cfx extension)
- Base libs: table, string, math, utf8 (io/os/package/debug intentionally
  not opened by the MyMP runtime for sandboxing)

## Tests

`tests/script_rt_test.cpp` (13 checks) runs the real demo `client.lua` and a
LuaGLM math script against this VM on Linux:
`-DLUA_INCLUDE_LIBGLM /tmp/liblua-cfx.a -ldl -lm`
