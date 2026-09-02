// lua_native_bindings.h — GENERATED declarations for lua_native_bindings.cpp
// (see tools/gen_native_bindings.py). Do not edit.
#pragma once
#include <cstddef>
#include <cstdint>
struct lua_State;
typedef int (*LuaNativeFn)(lua_State*);
struct LuaNativeEntry {
    const char* name;     // SNAKE_CASE (or _0x…) — lookup name
    const char* docName;  // FiveM-style TitleCase — global alias
    uint64_t hash;
    LuaNativeFn fn;
};
extern const LuaNativeEntry LUA_NATIVES[];
static constexpr size_t LUA_NATIVES_COUNT = 6302;
LuaNativeFn luaNativeByName(const char* name, uint64_t* hashOut);
LuaNativeFn luaNativeByHash(uint64_t hash);

// Vector3 marshaling helpers (implemented in script_rt.cpp using the
// vendored LuaGLM runtime so natives speak vec3 userdata like FiveM).
void luaNativePushVec3(lua_State* L, float x, float y, float z);
int  luaNativeToVec3(lua_State* L, int idx, float* x, float* y, float* z);
