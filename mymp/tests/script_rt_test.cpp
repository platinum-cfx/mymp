// tests/script_rt_test.cpp — Linux host-side test of the client scripting
// runtime: loads the real demo client.lua, dispatches server events, verifies
// mymp.send / mymp.print / mymp.native (via a mock native table) end-to-end.
//
// Build (from repo root):
//   g++ -O1 -std=c++17 -DLUA_INCLUDE_LIBGLM \
//       -Iclient/src -Iclient/src/lua -Iclient/src/lua/libs \
//       -Iclient/src/lua/libs/glm-binding tests/script_rt_test.cpp \
//       client/src/script_rt.cpp client/src/json.cpp \
//       client/src/lua_native_bindings.cpp \
//       /tmp/liblua-cfx.a -o /tmp/script_rt_test -ldl -lm
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

// mock Vector3 layout (matches LuaVector3 in the generated bindings)
struct MockVec3 { float x, y, z; };

// mock GET_ENTITY_COORDS: writes fixed coords through the hidden out-ptr
static void mockGetEntityCoords(uint64_t entity, uint64_t alive, MockVec3* out) {
    (void)entity; (void)alive;
    if (out) { out->x = 1.5f; out->y = -2.5f; out->z = 3.25f; }
}

// mock GET_PED_LAST_WEAPON_IMPACT_COORD: fills the vec3 in place, returns 1
static uint64_t mockLastWeaponImpact(uint64_t ped, MockVec3* out) {
    (void)ped;
    if (out) { out->x = 7.f; out->y = 8.f; out->z = 9.f; }
    return 1ull;
}

// mock GET_DISPLAY_NAME_FROM_VEHICLE_MODEL: returns a string
static const char* mockDisplayName(uint64_t model) {
    (void)model;
    return "ADDER";
}

// mock GET_GROUND_Z_FOR_3D_COORD: float* out-param
static uint64_t mockGroundZ(float x, float y, float z, float* out) {
    (void)x; (void)y; (void)z;
    if (out) *out = 25.5f;
    return 1ull;
}

// mock GET_CURRENT_PED_WEAPON: Hash* out-param
static uint64_t mockCurrentWeapon(uint64_t ped, int32_t* out, uint64_t p2) {
    (void)ped; (void)p2;
    if (out) *out = 0x2BE6766B;  // WEAPON_PISTOL
    return 1ull;
}

// mock GET_VEHICLE_LIGHTS_STATE: two BOOL* out-params
static uint64_t mockLightsState(uint64_t vehicle, int32_t* on, int32_t* hi) {
    (void)vehicle;
    if (on) *on = 1;
    if (hi) *hi = 0;
    return 1ull;
}

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
    // install mocks through the slot union (function-pointer bit cast)
    auto putSlot = [](uint64_t hash, uint64_t (*fn)()) {
        g_nativeTable[(hash >> 8) & 0xFF][hash & 0xFF] = fn;
    };
    union { void (*vec3get)(uint64_t, uint64_t, MockVec3*); uint64_t (*slot)(); } ug;
    ug.vec3get = mockGetEntityCoords;
    putSlot(0x3FEF770D40960D5AULL, ug.slot);            // GET_ENTITY_COORDS
    union { uint64_t (*impact)(uint64_t, MockVec3*); uint64_t (*slot)(); } uw;
    uw.impact = mockLastWeaponImpact;
    putSlot(0x6C4D0409BA1A2BC2ULL, uw.slot);            // GET_PED_LAST_WEAPON_IMPACT_COORD
    union { const char* (*dn)(uint64_t); uint64_t (*slot)(); } ud;
    ud.dn = mockDisplayName;
    putSlot(0xB215AAC32D25D019ULL, ud.slot);            // GET_DISPLAY_NAME_FROM_VEHICLE_MODEL
    union { uint64_t (*gz)(float, float, float, float*); uint64_t (*slot)(); } uGz;
    uGz.gz = mockGroundZ;
    putSlot(0xC906A7DAB05C8D2BULL, uGz.slot);            // GET_GROUND_Z_FOR_3D_COORD
    union { uint64_t (*cw)(uint64_t, int32_t*, uint64_t); uint64_t (*slot)(); } uCw;
    uCw.cw = mockCurrentWeapon;
    putSlot(0x3A87E44BB9A01D54ULL, uCw.slot);            // GET_CURRENT_PED_WEAPON
    union { uint64_t (*ls)(uint64_t, int32_t*, int32_t*); uint64_t (*slot)(); } uLs;
    uLs.ls = mockLightsState;
    putSlot(0xB91B4C20085BD12FULL, uLs.slot);            // GET_VEHICLE_LIGHTS_STATE
    printf("mock slots installed (vec3/string/float/hash/bool outs)\n");
    printf("mock slots installed for GET_ENTITY_COORDS + last-weapon-impact\n");

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

    // ---- native cache: FiveM-style direct global calls + vec3 marshaling ----
    {
        std::string code4 =
            "local ped = PlayerPedId()\n"                       // FiveM-style global
            "assert(ped == 42, 'PlayerPedId global')"
            "\nassert(PLAYER_PED_ID ~= nil, 'snake-case alias also global')"
            "\nlocal c = GetEntityCoords(ped, false)"           // vec3 return
            "\nassert(type(c) == 'userdata' or c.x ~= nil)"
            "\nassert(math.abs(c.x - 1.5) < 1e-4 and math.abs(c.y + 2.5) < 1e-4"
            " and math.abs(c.z - 3.25) < 1e-4, 'GetEntityCoords vec3')"
            "\nlocal ok, hit = GetPedLastWeaponImpactCoord(ped)" // vec3* out -> 2nd return
            "\nassert(ok == true, 'impact bool')"
            "\nassert(math.abs(hit.x - 7) < 1e-4 and math.abs(hit.y - 8) < 1e-4"
            " and math.abs(hit.z - 9) < 1e-4, 'impact vec3 out-param returned')"
            "\nmymp.print('native cache OK: ped=' .. tostring(ped) .. ' coords=' .. tostring(c))\n";
        std::string err4;
        check(rt.loadResource("nativecache", code4, err4), "native cache: globals + vec3 in/out");
        if (!err4.empty()) printf("  err: %s\n", err4.c_str());
    }
    {
        // hash-based dispatch still works alongside the globals
        // (64-bit hashes must be passed as strings — Lua numbers are doubles)
        std::string code5 =
            "local ped = mymp.native('0xD80958FC74E988A6')\n"
            "assert(ped == 42, 'mymp.native hash')"
            "\nlocal ped2 = mymp.native('PLAYER_PED_ID')"
            "\nassert(ped2 == 42, 'mymp.native name')\n";
        std::string err5;
        check(rt.loadResource("nativecache2", code5, err5), "mymp.native hash+name still work");
        if (!err5.empty()) printf("  err: %s\n", err5.c_str());
    }
    {
        // pointer out-params (string/float*/Hash*/BOOL*) come back as extra
        // return values, FiveM-style
        std::string code6 =
            "local name = GetDisplayNameFromVehicleModel(0xB779A091)\n"
            "assert(name == 'ADDER', 'string return')"
            "\nlocal okz, z = GetGroundZFor_3dCoord(0, 0, 0, 0.0)"
            "\nassert(okz == true and math.abs(z - 25.5) < 1e-4, 'float* out-param')"
            "\nlocal okw, wh = GetCurrentPedWeapon(42, 0, true)"
            "\nassert(okw == true and wh == 0x2BE6766B, 'Hash* out-param')"
            "\nlocal ret, on, hi = GetVehicleLightsState(42)"
            "\nassert(ret == true and on == true and hi == false, 'BOOL* out-params')\n";
        std::string err6;
        check(rt.loadResource("ptrs", code6, err6), "pointer out-params return extra values");
        if (!err6.empty()) printf("  err: %s\n", err6.c_str());
    }

    printf(g_failures ? "\nSCRIPT RT TEST FAILED (%d)\n" : "\nSCRIPT RT TEST PASSED\n",
           g_failures);
    return g_failures ? 1 : 0;
}
