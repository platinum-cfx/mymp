// lua_native_bindings.h — GENERATED declarations for lua_native_bindings.cpp
// (see tools/gen_native_bindings.py). Do not edit.
#pragma once
#include <cstddef>
#include <cstdint>
struct lua_State;
typedef int (*LuaNativeFn)(lua_State*);
static constexpr size_t LUA_NATIVES_COUNT = 6302;
LuaNativeFn luaNativeByName(const char* name, uint64_t* hashOut);
LuaNativeFn luaNativeByHash(uint64_t hash);
