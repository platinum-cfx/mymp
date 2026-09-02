// script_rt.cpp — MyMP client-side scripting runtime (Lua 5.4)
// Original code. Lua itself is MIT-licensed (client/src/lua/LICENSE).
#include "script_rt.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "lua/lua.h"
#include "lua/lauxlib.h"
#include "lua/lualib.h"
#if defined(LUA_INCLUDE_LIBGLM)
extern "C" int luaopen_glm(lua_State*);
#endif

#include "lua_native_bindings.h"
#include "net.h"

namespace mymp {

// ---------------------------------------------------------------------------
// JSON <-> Lua
// ---------------------------------------------------------------------------

static void jsonAppend(std::string& out, lua_State* L, int idx) {
    int t = lua_type(L, idx);
    switch (t) {
        case LUA_TNIL: out += "null"; break;
        case LUA_TBOOLEAN: out += lua_toboolean(L, idx) ? "true" : "false"; break;
        case LUA_TNUMBER: {
            char buf[48];
            lua_Number n = lua_tonumber(L, idx);
            if (n == (lua_Number)(lua_Integer)n && n < 9.2e18 && n > -9.2e18)
                snprintf(buf, sizeof buf, "%lld", (long long)n);
            else
                snprintf(buf, sizeof buf, "%.10g", (double)n);
            out += buf;
            break;
        }
        case LUA_TSTRING: {
            size_t len;
            const char* s = lua_tolstring(L, idx, &len);
            out += '"';
            for (size_t i = 0; i < len; ++i) {
                char c = s[i];
                if (c == '"' || c == '\\') { out += '\\'; out += c; }
                else if (c == '\n') out += "\\n";
                else if (c == '\r') out += "\\r";
                else if (c == '\t') out += "\\t";
                else if ((unsigned char)c < 0x20) {
                    char buf[8]; snprintf(buf, sizeof buf, "\\u%04x", c);
                    out += buf;
                } else out += c;
            }
            out += '"';
            break;
        }
        case LUA_TTABLE: {
            bool array = true;
            lua_Integer n = 0;
            for (lua_pushnil(L); lua_next(L, idx); lua_pop(L, 1)) {
                if (lua_type(L, -2) != LUA_TNUMBER ||
                    lua_tointeger(L, -2) != ++n) { array = false; break; }
            }
            if (array) {
                out += '[';
                lua_Integer len = (lua_Integer)lua_rawlen(L, idx);
                for (lua_Integer i = 1; i <= len; ++i) {
                    if (i > 1) out += ',';
                    lua_rawgeti(L, idx, (int)i);
                    jsonAppend(out, L, -1);
                    lua_pop(L, 1);
                }
                out += ']';
            } else {
                out += '{';
                bool first = true;
                for (lua_pushnil(L); lua_next(L, idx); lua_pop(L, 1)) {
                    if (!first) out += ',';
                    first = false;
                    jsonAppend(out, L, -2);
                    out += ':';
                    jsonAppend(out, L, -1);
                }
                out += '}';
            }
            break;
        }
        default: out += "null"; break;
    }
}

static bool luaToJson(lua_State* L, int idx, std::string& out) {
    out.clear();
    jsonAppend(out, L, idx);
    return !out.empty();
}

// Push a Lua table from a JSON object string ("" -> empty table).
void ScriptRuntime::pushJsonObject(lua_State* L, const std::string& dataJson) {
    if (dataJson.empty()) {
        lua_newtable(L);
        return;
    }
    mymp::Json j;
    if (!j.parse(dataJson) || j.type != mymp::Json::OBJ) {
        lua_newtable(L);
        return;
    }
    lua_newtable(L);
    for (const auto& kv : j.obj) {
        lua_pushstring(L, kv.first.c_str());
        switch (kv.second.type) {
            case mymp::Json::NUL: lua_pushnil(L); break;
            case mymp::Json::BOOL: lua_pushboolean(L, kv.second.b ? 1 : 0); break;
            case mymp::Json::NUM: lua_pushnumber(L, kv.second.num); break;
            case mymp::Json::STR: lua_pushstring(L, kv.second.str.c_str()); break;
            case mymp::Json::ARR: {
                lua_newtable(L);
                int i = 1;
                for (const mymp::Json& e : kv.second.arr) {
                    if (e.type == mymp::Json::NUM) {
                        lua_pushnumber(L, e.num); lua_rawseti(L, -2, i++);
                    } else if (e.type == mymp::Json::STR) {
                        lua_pushstring(L, e.str.c_str()); lua_rawseti(L, -2, i++);
                    } else if (e.type == mymp::Json::BOOL) {
                        lua_pushboolean(L, e.b ? 1 : 0); lua_rawseti(L, -2, i++);
                    }
                }
                break;
            }
            default: lua_pushnil(L); break;
        }
        lua_settable(L, -3);
    }
}

// ---------------------------------------------------------------------------
// the `mymp` API
// ---------------------------------------------------------------------------

int ScriptRuntime::luaPrint(lua_State* L) {
    ScriptRuntime* rt = (ScriptRuntime*)lua_touserdata(L, lua_upvalueindex(1));
    std::string text;
    int n = lua_gettop(L);
    for (int i = 1; i <= n; ++i) {
        if (i > 1) text += " ";
        size_t len;
        const char* s = luaL_tolstring(L, i, &len);
        text.append(s, len);
        lua_pop(L, 1);
    }
    if (rt && rt->host.print) rt->host.print(text.c_str());
    return 0;
}

int ScriptRuntime::luaOn(lua_State* L) {
    const char* event = luaL_checkstring(L, 1);
    luaL_checktype(L, 2, LUA_TFUNCTION);
    lua_getfield(L, LUA_REGISTRYINDEX, "mymp_handlers");   // handlers
    lua_getfield(L, -1, event);                             // handlers[event]
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_setfield(L, -3, event);
    }
    int n = (int)lua_rawlen(L, -1);
    lua_pushvalue(L, 2);
    lua_rawseti(L, -2, n + 1);
    lua_pop(L, 2);
    return 0;
}

int ScriptRuntime::luaSend(lua_State* L) {
    ScriptRuntime* rt = (ScriptRuntime*)lua_touserdata(L, lua_upvalueindex(1));
    const char* name = luaL_checkstring(L, 1);
    if (!rt || !rt->host.sendEvent || !name) return 0;
    std::string json;
    if (lua_gettop(L) >= 2 && !lua_isnil(L, 2))
        luaToJson(L, 2, json);
    rt->host.sendEvent(name, json.c_str());
    return 0;
}

int ScriptRuntime::luaNative(lua_State* L) {
    const char* target = luaL_checkstring(L, 1);
    if (!target || !*target) return luaL_error(L, "mymp.native: empty name");
    uint64_t hash = 0;
    if (target[0] == '0' && (target[1] == 'x' || target[1] == 'X'))
        hash = strtoull(target, nullptr, 16);
    lua_remove(L, 1);
    if (hash) {
        LuaNativeFn fn = luaNativeByHash(hash);
        if (!fn) return luaL_error(L, "mymp.native: unknown hash %s", target);
        return fn(L);
    }
    LuaNativeFn fn = luaNativeByName(target, nullptr);
    if (!fn) return luaL_error(L, "mymp.native: unknown native '%s'", target);
    return fn(L);
}

int ScriptRuntime::luaNativesCount(lua_State* L) {
    lua_pushinteger(L, (lua_Integer)LUA_NATIVES_COUNT);
    return 1;
}

int ScriptRuntime::luaVersion(lua_State* L) {
    lua_pushstring(L, "MyMP client scripting 1.0 (Lua 5.4)");
    return 1;
}

// ---------------------------------------------------------------------------
// runtime
// ---------------------------------------------------------------------------

ScriptRuntime::~ScriptRuntime() {
    shutdown();
}

bool ScriptRuntime::init(const ScriptHost& host) {
    if (L) shutdown();
    this->host = host;
    L = luaL_newstate();
    if (!L) return false;
    openLibraries(L);
    registerApi(L);
    lua_newtable(L);
    lua_setfield(L, LUA_REGISTRYINDEX, "mymp_handlers");
    return true;
}

void ScriptRuntime::shutdown() {
    if (L) {
        lua_close(L);
        L = nullptr;
    }
}

void ScriptRuntime::openLibraries(lua_State* L) {
    // Safe core only: no io/os/package/debug, so server-pushed scripts cannot
    // touch the filesystem or the process.
    static const luaL_Reg libs[] = {
        {"_G", luaopen_base},
        {LUA_TABLIBNAME, luaopen_table},
        {LUA_STRLIBNAME, luaopen_string},
        {LUA_MATHLIBNAME, luaopen_math},
        {LUA_UTF8LIBNAME, luaopen_utf8},
        {nullptr, nullptr},
    };
    for (const luaL_Reg* l = libs; l->func; ++l) {
        luaL_requiref(L, l->name, l->func, 1);
        lua_pop(L, 1);
    }
    lua_pushnil(L); lua_setglobal(L, "dofile");
    lua_pushnil(L); lua_setglobal(L, "loadfile");
    lua_pushnil(L); lua_setglobal(L, "print");
    // LuaGLM (the Cfx Lua extension): opening the glm library installs the
    // vector/matrix metatables (so vec3:length() etc. work). The vector
    // constructors (vec3, quat, mat4, ...) are globals registered by the
    // base library itself, exactly like FiveM.
#if defined(LUA_INCLUDE_LIBGLM)
    luaL_requiref(L, "glm", luaopen_glm, 1);
    lua_pop(L, 1);
#endif
}

void ScriptRuntime::registerApi(lua_State* L) {
    lua_newtable(L);                            // mymp table
    lua_pushlightuserdata(L, this);             // upvalue for print
    lua_pushcclosure(L, luaPrint, 1);           // consumes the upvalue
    lua_setfield(L, -2, "print");
    lua_pushcclosure(L, luaOn, 0);
    lua_setfield(L, -2, "on");
    lua_pushlightuserdata(L, this);             // upvalue for send
    lua_pushcclosure(L, luaSend, 1);
    lua_setfield(L, -2, "send");
    lua_pushcclosure(L, luaNative, 0);
    lua_setfield(L, -2, "native");
    lua_pushcclosure(L, luaNativesCount, 0);
    lua_setfield(L, -2, "nativesCount");
    lua_pushcclosure(L, luaVersion, 0);
    lua_setfield(L, -2, "version");
    lua_setglobal(L, "mymp");
}

bool ScriptRuntime::loadResource(const std::string& resource,
                                 const std::string& source, std::string& err) {
    if (!L) return false;
    // Per-resource environment table chained to _G (no global collisions).
    lua_newtable(L);                        // env
    lua_newtable(L);                        // meta
    lua_pushliteral(L, "__index");
    lua_rawgeti(L, LUA_REGISTRYINDEX, LUA_RIDX_GLOBALS);
    lua_rawset(L, -3);                      // meta.__index = _G
    lua_pushvalue(L, -1);                   // meta
    lua_setmetatable(L, -3);                // setmetatable(env, meta); pops meta
    if (luaL_loadbuffer(L, source.c_str(), source.size(), resource.c_str())) {
        err = lua_tostring(L, -1) ? lua_tostring(L, -1) : "syntax error";
        lua_pop(L, 3);
        return false;
    }
    // chunk's first upvalue (_ENV) := env
    lua_pushvalue(L, -3);
    lua_setupvalue(L, -2, 1);
    if (lua_pcall(L, 0, 0, 0)) {
        err = lua_tostring(L, -1) ? lua_tostring(L, -1) : "runtime error";
        lua_pop(L, 2);
        return false;
    }
    lua_pop(L, 2);                          // env, chunk
    return true;
}

void ScriptRuntime::dispatch(const std::string& event, const std::string& dataJson) {
    if (!L) return;
    lua_getfield(L, LUA_REGISTRYINDEX, "mymp_handlers");
    lua_getfield(L, -1, event.c_str());     // handlers[event] or nil
    if (lua_isnil(L, -1)) {
        lua_pop(L, 2);
        return;
    }
    pushJsonObject(L, dataJson);            // payload
    int n = (int)lua_rawlen(L, -2);
    for (int i = 1; i <= n; ++i) {
        lua_rawgeti(L, -2, i);              // fn
        lua_pushvalue(L, -2);               // payload
        if (lua_pcall(L, 1, 0, 0)) {
            const char* msg = lua_tostring(L, -1);
            if (host.print && msg) host.print(msg);
            lua_pop(L, 1);
        }
    }
    lua_pop(L, 2);
}

}  // namespace mymp
