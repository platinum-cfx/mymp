// script_rt.h — MyMP client-side scripting runtime (Lua 5.4)
//
// Runs Lua resource files INSIDE GTA V, the same idea as FiveM's client
// scripts. All calls must happen on ONE thread (the client's network/main
// thread). The API surface exposed to scripts (the `mymp` table):
//
//   mymp.print(...)                 in-game help text
//   mymp.on(event, fn)              register an event handler
//   mymp.send(name, data)           fire a network event to the server
//   mymp.native(nameOrHash, ...)    call ANY GTA V native (typed bindings)
//   mymp.nativesCount()             number of bound natives
//
// Events dispatched from the game: "chat" {name,msg}, "sys" {msg},
// "join"/"leave" {id,name}, "spawn" {x,y,h}, "damage" {target,by,amount},
// "death" {id,by}, "respawn" {id,x,y}, "announce"/"pm" {msg}, plus any
// custom server event by its name with its data table.
#pragma once
#include <string>

struct lua_State;

namespace mymp {

struct ScriptHost {
    void (*print)(const char* text);                    // in-game text overlay
    void (*sendEvent)(const char* name, const char* dataJson);  // UDP to server
};

class ScriptRuntime {
public:
    ScriptRuntime() = default;
    ~ScriptRuntime();

    bool init(const ScriptHost& host);
    void shutdown();

    // Load a resource's Lua source; returns false + error message on failure.
    bool loadResource(const std::string& resource, const std::string& source,
                      std::string& err);

    // Dispatch a game event (payload is a JSON object string, may be empty).
    void dispatch(const std::string& event, const std::string& dataJson);

    bool valid() const { return L != nullptr; }

private:
    static int luaPrint(lua_State* L);
    static int luaOn(lua_State* L);
    static int luaSend(lua_State* L);
    static int luaNative(lua_State* L);
    static int luaNativesCount(lua_State* L);
    static int luaVersion(lua_State* L);

    void openLibraries(lua_State* L);
    void registerApi(lua_State* L);
    static void pushJsonObject(lua_State* L, const std::string& dataJson);

    lua_State* L = nullptr;
    ScriptHost host{};
};

}  // namespace mymp
