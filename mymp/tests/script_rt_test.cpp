// tests/script_rt_test.cpp — Linux host-side test of the client scripting
// runtime: loads the real demo client.lua, dispatches server events, verifies
// mymp.send / mymp.print / mymp.native (via a mock native table) end-to-end.
//
// Build (from repo root):
//   g++ -O1 -std=c++17 -Iclient/src tests/script_rt_test.cpp \
//       client/src/script_rt.cpp client/src/json.cpp \
//       client/src/lua_native_bindings.cpp \
//       -DLUA_INCLUDE_LIBGLM /tmp/liblua-cfx.a -o /tmp/script_rt_test -ldl -lm
// (liblua-cfx.a = the vendored Cfx Lua 5.4.4 + LuaGLM, built as C++ like
//  FiveM: g++ -c -x c++ -std=c++17 -DLUA_INCLUDE_LIBGLM [flags] l*.c lglm.cpp
//  libs/glm-binding/lglmlib.cpp)
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "script_rt.h"
#include "natives.h"            // invoke<> + g_nativeTable
#include "natives_full.h"       // nativeHashByName
#include "net.h"

// ---- mock native table (client.cpp defines this on Windows) ----
uint64_t (*g_nativeTable[256][256])();

// mock native: PLAYER_PED_ID returns 42
static uint64_t mockPlayerPedId() { return 42ull; }

static std::vector<std::string> g_prints;
static std::vector<std::pair<std::string, std::string>> g_sent;
static int g_failures = 0;

static void onPrint(const char* t) { g_prints.push_back(t ? t : ""); }
static void onSend(const char* name, const char* data) {
    g_sent.push_back({name ? name : "", data ? data : ""});
}

static void check(bool cond, const char* what) {
    printf("%s %s\n", cond ? "PASS" : "FAIL", what);
    if (!cond) ++g_failures;
}

int main() {
    // point PLAYER_PED_ID's slot at a mock
    uint64_t h = nativeHashByName("PLAYER_PED_ID");
    if (!h) { printf("FAIL nativeHashByName(PLAYER_PED_ID)\n"); return 1; }
    g_nativeTable[(h >> 8) & 0xFF][h & 0xFF] = mockPlayerPedId;
    printf("mock slot installed for PLAYER_PED_ID 0x%016llX\n",
           (unsigned long long)h);

    mymp::ScriptHost host;
    host.print = onPrint;
    host.sendEvent = onSend;
    mymp::ScriptRuntime rt;
    check(rt.init(host), "runtime init");
    check(rt.valid(), "runtime valid");

    // load the real demo script from the repo
    FILE* f = fopen("server/plugins/scriptdemo/client.lua", "rb");
    if (!f) { printf("FAIL open demo client.lua\n"); return 1; }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::string src((size_t)sz, '\0');
    fread(&src[0], 1, (size_t)sz, f);
    fclose(f);

    std::string err;
    check(rt.loadResource("scriptdemo/client.lua", src, err), "load demo script");
    if (!err.empty()) printf("  load err: %s\n", err.c_str());
    check(g_prints.size() == 1 &&
          g_prints[0] == "[scriptdemo] client.lua loaded inside GTA V",
          "script ran on load (mymp.print)");

    // server sends demo:hello -> script replies demo:pong
    rt.dispatch("demo:hello", "{\"msg\":\"welcome to MyMP scripting\"}");
    check(g_prints.size() >= 2 &&
          g_prints[1] == "[scriptdemo] server says: welcome to MyMP scripting",
          "demo:hello handler ran");
    check(g_sent.size() == 1 && g_sent[0].first == "demo:pong" &&
          g_sent[0].second.find("\"msg\":\"pong from GTA V\"") != std::string::npos,
          "mymp.send fired demo:pong with JSON payload");

    // chat event with !scriptdemo -> demo:triggered
    rt.dispatch("chat", "{\"name\":\"Bob\",\"msg\":\"!scriptdemo\"}");
    check(g_sent.size() == 2 && g_sent[1].first == "demo:triggered" &&
          g_sent[1].second.find("\"by\":\"Bob\"") != std::string::npos,
          "chat handler fired demo:triggered");

    // an event with no handler must be a no-op
    rt.dispatch("nobody:listens", "{\"a\":1}");

    // mymp.native through the binding + mock table
    {
        // call the runtime via a tiny embedded script using the API:
        // we re-init a fresh runtime so we can run an inline script
        mymp::ScriptRuntime rt2;
        rt2.init(host);
        std::string code =
            "local ped = mymp.native(\"PLAYER_PED_ID\")\n"
            "if ped ~= 42 then error(\"expected 42, got \" .. tostring(ped)) end\n"
            "if mymp.nativesCount() < 6000 then error(\"count too small\") end\n"
            "local v = mymp.native(\"0x4F8644AF03D0E0D6\")\n"  // PLAYER_ID (no mock -> 0)
            "mymp.print(\"native-ok count=\" .. mymp.nativesCount())\n";
        std::string err2;
        check(rt2.loadResource("inline", code, err2), "inline native script loads");
        if (!err2.empty()) printf("  err: %s\n", err2.c_str());
        bool printed = false;
        for (const auto& pr : g_prints)
            if (pr.find("native-ok count=6302") != std::string::npos) printed = true;
        check(printed, "mymp.native returned mock value, count=6302");
    }

    // unknown native must error, not crash
    {
        mymp::ScriptRuntime rt3;
        rt3.init(host);
        std::string err3;
        std::string code3 = "local ok = pcall(function() mymp.native(\"NOT_A_NATIVE\") end)\n"
                            "if not ok then mymp.print(\"unknown-native-errors-ok\") end\n";
        check(rt3.loadResource("inline3", code3, err3), "unknown native pcall script");
        bool printed = false;
        for (const auto& pr : g_prints)
            if (pr.find("unknown-native-errors-ok") != std::string::npos) printed = true;
        check(printed, "unknown native raised a catchable Lua error");
    }

    // ---- LuaGLM vector math (the FiveM vec3/quat/mat4 extension) ----
    {
        std::string code3 = "local v = vec3(1, 2, 3)\n"
                            "assert(tostring(v) == 'vec3(1.000000, 2.000000, 3.000000)')"
                            "\nassert(math.abs(v:length() - 3.741657) < 1e-4)"
                            "\nassert(v + vec3(1,0,0) == vec3(2,2,3))"
                            "\nassert(v.x == 1 and v.y == 2 and v.z == 3)"
                            "\nassert(quat() ~= nil and mat4() ~= nil)"
                            "\nlocal m = mat4()"
                            "\nassert(tostring(m):find('mat4x4') ~= nil)"
                            "\nmymp.print('vec3 math OK: ' .. tostring(v))\n";
        std::string err3;
        check(rt.loadResource("vec3test", code3, err3), "LuaGLM vec3 math runs");
    }

    printf(g_failures ? "\nSCRIPT RT TEST FAILED (%d)\n" : "\nSCRIPT RT TEST PASSED\n",
           g_failures);
    return g_failures ? 1 : 0;
}
