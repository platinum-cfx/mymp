// client.cpp — MyMP GTA V client (original code, written for MyMP)
//
// Builds to MyMP.asi — a DLL that an ASI loader (e.g. Ultimate ASI Loader)
// injects into GTA5.exe. The client:
//   1. discovers GTA V's native function table inside the game process,
//   2. connects to your MyMP server over UDP,
//   3. spawns your vehicle, streams your position to the server,
//   4. spawns/moves vehicles for other players and AI bots.
//
// Compile on Windows:  see build.ps1 (one command with VS Build Tools).

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <psapi.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <deque>
#include <cmath>

#include "natives.h"
#include "net.h"
#include "scriptthread.h"

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "psapi.lib")

// ==================== globals ====================
uint64_t (*g_nativeTable[256][256])() = {};  // discovered at runtime

namespace {

using mymp::Json;
using mymp::UdpSocket;

// ---------- config ----------
struct Config {
    std::string host = "127.0.0.1";
    uint16_t port = 30120;
    std::string name = "GTA-Player";
    std::string vehicle = "adder";
    std::string license;
    int r = 255, g = 159, b = 28;  // default orange
};
Config g_cfg;
std::string g_gameDir;
std::string g_logPath;
bool g_quit = false;

void logLine(const std::string& s) {
    FILE* f = fopen(g_logPath.c_str(), "a");
    if (f) {
        SYSTEMTIME st;
        GetLocalTime(&st);
        fprintf(f, "[%02d:%02d:%02d] %s\n", st.wHour, st.wMinute, st.wSecond, s.c_str());
        fclose(f);
    }
}

// ---------- joaat (model hash) ----------
uint32_t joaat(const std::string& s) {
    uint32_t h = 0;
    for (char c : s) {
        h += (unsigned char)c;
        h += h << 10;
        h ^= h >> 6;
    }
    h += h << 3;
    h ^= h >> 11;
    h += h << 15;
    return h;
}

// ---------- native table discovery ----------
// GTA5.exe (x64) holds a 256x256 table of native function pointers. We scan
// the module for `lea rax,[rip+disp32]` instructions and validate candidates
// by checking that the slots for two known natives are code pointers inside
// the module. This is version-robust and needs no hardcoded offsets.
uint8_t* findNativeTable() {
    uint8_t* base = (uint8_t*)GetModuleHandleA("GTA5.exe");
    if (!base) return nullptr;
    IMAGE_DOS_HEADER* dos = (IMAGE_DOS_HEADER*)base;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return nullptr;
    IMAGE_NT_HEADERS* nt = (IMAGE_NT_HEADERS*)(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return nullptr;

    uint8_t* codeStart = base + nt->OptionalHeader.BaseOfCode;
    size_t codeSize = nt->OptionalHeader.SizeOfCode;
    uintptr_t modStart = (uintptr_t)base;
    uintptr_t modEnd = modStart + nt->OptionalHeader.SizeOfImage;
    size_t tblBytes = 256 * 256 * sizeof(uint64_t);

    for (size_t off = 0; off + 7 <= codeSize; ++off) {
        uint8_t* p = codeStart + off;
        if (p[0] != 0x48 || p[1] != 0x8D || p[2] != 0x05) continue;  // lea rax,[rip+disp]
        int32_t disp;
        memcpy(&disp, p + 3, 4);
        uint8_t* target = p + 7 + disp;
        if ((uintptr_t)target < modStart || (uintptr_t)target + tblBytes > modEnd) continue;

        uint64_t(*tbl)[256] = (uint64_t(*)[256])target;
        auto slot = [&](uint64_t hash) -> uint64_t {
            return tbl[(hash >> 8) & 0xFF][hash & 0xFF];
        };
        uint64_t s1 = slot(N_PLAYER_PED_ID);
        uint64_t s2 = slot(N_GET_ENTITY_COORDS);
        if (s1 >= modStart && s1 < modEnd && (s1 & 7) == 0 &&
            s2 >= modStart && s2 < modEnd && (s2 & 7) == 0) {
            return target;
        }
    }
    return nullptr;
}

// ---------- config file ----------
void loadConfig() {
    std::string path = g_gameDir + "mymp.ini";
    FILE* f = fopen(path.c_str(), "r");
    if (!f) return;
    char line[256];
    std::string section;
    while (fgets(line, sizeof line, f)) {
        std::string s = line;
        // strip \r\n and comments
        size_t cut = s.find_first_of("\r\n#;");
        if (cut != std::string::npos) s = s.substr(0, cut);
        s.erase(0, s.find_first_not_of(" \t"));
        while (!s.empty() && (s.back() == ' ' || s.back() == '\t')) s.pop_back();
        if (s.empty()) continue;
        if (s.front() == '[' && s.back() == ']') { section = s.substr(1, s.size() - 2); continue; }
        size_t eq = s.find('=');
        if (eq == std::string::npos) continue;
        std::string k = s.substr(0, eq), v = s.substr(eq + 1);
        // trim
        k.erase(0, k.find_first_not_of(" \t")); while (!k.empty() && (k.back() == ' ' || k.back() == '\t')) k.pop_back();
        v.erase(0, v.find_first_not_of(" \t")); while (!v.empty() && (v.back() == ' ' || v.back() == '\t')) v.pop_back();
        if (section == "server") {
            if (k == "host") g_cfg.host = v;
            else if (k == "port") g_cfg.port = (uint16_t)atoi(v.c_str());
        } else if (section == "player") {
            if (k == "name") g_cfg.name = v;
            else if (k == "vehicle") g_cfg.vehicle = v;
            else if (k == "color" && v.size() == 7 && v[0] == '#') {
                g_cfg.r = (int)strtol(v.substr(1, 2).c_str(), nullptr, 16);
                g_cfg.g = (int)strtol(v.substr(3, 2).c_str(), nullptr, 16);
                g_cfg.b = (int)strtol(v.substr(5, 2).c_str(), nullptr, 16);
            }
            else if (k == "license") g_cfg.license = v;
        }
    }
    // install license identifier (Cfx-style): generate once, persist, and the
    // server keys your account on it so your save follows your install.
    if (g_cfg.license.empty()) {
        char lic[32];
        HKEY hk;
        if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\MyMP", 0, KEY_READ | KEY_WRITE, &hk) == ERROR_SUCCESS) {
            DWORD sz = sizeof lic; DWORD type = REG_SZ;
            if (RegQueryValueExA(hk, "license", NULL, &type, (BYTE*)lic, &sz) == ERROR_SUCCESS &&
                lic[0] && strlen(lic) == 24) {
                g_cfg.license = lic;
            } else {
                const char* hex = "0123456789abcdef";
                for (int i = 0; i < 24; i++) lic[i] = hex[rand() % 16];
                lic[24] = 0;
                RegSetValueExA(hk, "license", 0, REG_SZ, (BYTE*)lic, 25);
                g_cfg.license = lic;
            }
            RegCloseKey(hk);
        } else {
            const char* hex = "0123456789abcdef";
            for (int i = 0; i < 24; i++) lic[i] = hex[rand() % 16];
            lic[24] = 0;
            g_cfg.license = lic;
        }
    }
    fclose(f);
}

// ---------- game state ----------
struct Vector3 { float x, y, z; };
bool g_joined = false;
bool g_ownVehSpawned = false;
uint32_t g_ownVeh = 0;  // entity handle
std::map<uint32_t, uint32_t> g_remoteVehs;  // server entity id -> game vehicle handle
std::map<uint32_t, float> g_remoteZs;       // server entity id -> last known z
std::map<uint32_t, uint32_t> g_remotePeds;  // server entity id -> game ped handle
std::map<uint32_t, float> g_remotePedZs;    // server entity id -> last known ped z

// remote-entity interpolation — design taken from GTANetworkDev/platform
// (MIT, Client/Sync/Interpolation.cs + Client/Util/Interpolator.cs): chase the
// latest snapshot on a short delay instead of snapping at 10 Hz; extrapolate
// along heading at speed when updates are late. Our own implementation.
struct LerpState {
    bool have = false;
    float x = 0, y = 0, z = 0, h = 0, spd = 0;
    uint64_t t = 0;
};
std::map<uint32_t, LerpState> g_vehLerp, g_pedLerp;
std::map<uint32_t, uint32_t> g_remoteObjs;  // server object id -> game object handle
std::map<uint32_t, std::string> g_remoteNames;  // id -> name (player list overlay)
std::map<uint32_t, int> g_remoteHp;             // id -> hp (player list overlay)
bool g_playerList = false;
UdpSocket g_sock;

// ---- in-game chat (FiveM-style: T opens, type, Enter sends, Esc closes) ----
uint32_t g_myId = 0;
bool g_chatOpen = false;
std::string g_chatBuf;
std::deque<std::string> g_chatLog;  // last 6 lines drawn on screen

std::string msgJoin() {
    char buf[640];
    snprintf(buf, sizeof buf,
             "{\"t\":\"join\",\"name\":\"%s\",\"color\":\"#%02X%02X%02X\",\"native\":1,\"lic\":\"%s\"}",
             mymp::jsonEscape(g_cfg.name).c_str(), g_cfg.r, g_cfg.g, g_cfg.b,
             g_cfg.license.c_str());
    return buf;
}

std::string msgState(float x, float y, float h, float s, uint32_t model, int foot,
                   int hp, int ar) {
    char buf[320];
    snprintf(buf, sizeof buf,
             "{\"t\":\"nat\",\"x\":%.1f,\"y\":%.1f,\"h\":%.3f,\"s\":%.1f,\"m\":%u,\"f\":%d,\"hp\":%d,\"ar\":%d}",
             x, y, h, s, model, foot, hp, ar);
    return buf;
}

std::string msgChat(const std::string& text) {
    return "{\"t\":\"chat\",\"msg\":\"" + mymp::jsonEscape(text) + "\"}";
}

// ---------- spawning ----------
uint32_t requestModelHash(uint32_t model) {
    invoke<void>(N_REQUEST_MODEL, (uint64_t)model);
    for (int i = 0; i < 200 && !invoke<bool>(N_HAS_MODEL_LOADED, (uint64_t)model); ++i)
        Sleep(20);
    return invoke<bool>(N_HAS_MODEL_LOADED, (uint64_t)model) ? model : 0;
}

uint32_t requestModel(const std::string& modelName) {
    uint32_t model = joaat(modelName);
    invoke<void>(N_REQUEST_MODEL, (uint64_t)model);
    for (int i = 0; i < 200 && !invoke<bool>(N_HAS_MODEL_LOADED, (uint64_t)model); ++i)
        Sleep(20);
    if (!invoke<bool>(N_HAS_MODEL_LOADED, (uint64_t)model)) return 0;
    return model;
}

uint32_t requestVehicleModel(const std::string& modelName) {
    uint32_t model = joaat(modelName);
    if (!invoke<bool>(N_IS_MODEL_A_VEHICLE, (uint64_t)model)) return 0;
    return requestModel(modelName);
}

void colorize(uint32_t veh) {
    invoke<void>(N_SET_VEHICLE_CUSTOM_PRIMARY_COLOUR, (uint64_t)veh, g_cfg.r, g_cfg.g, g_cfg.b);
    invoke<void>(N_SET_VEHICLE_CUSTOM_SECONDARY_COLOUR, (uint64_t)veh, g_cfg.r, g_cfg.g, g_cfg.b);
}

uint32_t spawnOwnVehicle(const std::string& modelName,
                         float x, float y, float z, float heading) {
    uint32_t model = requestVehicleModel(modelName);
    if (!model) return 0;
    uint32_t veh = invoke<uint32_t>(N_CREATE_VEHICLE, (uint64_t)model, x, y, z + 1.5f, heading, 0, 0);
    if (!veh) return 0;
    invoke<void>(N_SET_VEHICLE_ON_GROUND_PROPERLY, (uint64_t)veh, 0);
    invoke<void>(N_SET_VEHICLE_ENGINE_ON, (uint64_t)veh, 1, 1, 0);
    invoke<void>(N_SET_VEHICLE_IS_CONSIDERED_BY_PLAYER, (uint64_t)veh, 1);
    invoke<void>(N_SET_ENTITY_AS_MISSION_ENTITY, (uint64_t)veh, 1, 1, 1);
    colorize(veh);
    uint32_t ped = invoke<uint32_t>(N_PLAYER_PED_ID);
    if (ped) {
        invoke<void>(N_SET_PED_INTO_VEHICLE, (uint64_t)ped, (uint64_t)veh, -1);
        invoke<void>(N_CLEAR_PED_TASKS_IMMEDIATELY, (uint64_t)ped);
    }
    uint32_t player = invoke<uint32_t>(N_PLAYER_ID);
    invoke<void>(N_SET_PLAYER_WANTED_LEVEL, (uint64_t)player, 0, 0);
    return veh;
}

// spawn a vehicle for a remote entity; returns handle or 0
uint32_t spawnRemoteVehicle(const Json& e) {
    uint32_t model = 0;
    // real model streamed from the server (asset-streaming lite): you see the
    // actual car the other player drives, not a stand-in
    if (e.has("m")) {
        uint32_t m = (uint32_t)e.get("m")->asNum();
        if (m && invoke<bool>(N_IS_MODEL_A_VEHICLE, (uint64_t)m))
            model = requestModelHash(m);
    }
    if (!model) {  // fallback pool until the real model arrives
        static const char* pool[] = {"adder", "sultan", "futo", "banshee", "oracle", "buffalo"};
        uint32_t poolIdx = e.has("i") ? ((uint32_t)e.get("i")->num) % 6 : 0;
        model = requestVehicleModel(pool[poolIdx]);
    }
    if (!model) return 0;
    float x = e.has("x") ? (float)e.get("x")->asNum() : 0.0f;
    float y = e.has("y") ? (float)e.get("y")->asNum() : 0.0f;
    uint32_t veh = invoke<uint32_t>(N_CREATE_VEHICLE, (uint64_t)model, x, y, 200.0f, 0.0f, 0, 0);
    if (!veh) return 0;
    invoke<void>(N_SET_ENTITY_AS_MISSION_ENTITY, (uint64_t)veh, 1, 1, 1);
    invoke<void>(N_SET_ENTITY_COLLISION, (uint64_t)veh, 0, 1);  // ghost until needed
    invoke<void>(N_SET_VEHICLE_ON_GROUND_PROPERLY, (uint64_t)veh, 0);  // settle on terrain
    // tint from the entity colour if present
    if (e.has("c")) {
        std::string c = e.get("c")->asStr("#ffffff");
        if (c.size() == 7 && c[0] == '#') {
            int r = (int)strtol(c.substr(1, 2).c_str(), nullptr, 16);
            int gg = (int)strtol(c.substr(3, 2).c_str(), nullptr, 16);
            int b = (int)strtol(c.substr(5, 2).c_str(), nullptr, 16);
            invoke<void>(N_SET_VEHICLE_CUSTOM_PRIMARY_COLOUR, (uint64_t)veh, r, gg, b);
            invoke<void>(N_SET_VEHICLE_CUSTOM_SECONDARY_COLOUR, (uint64_t)veh, r, gg, b);
        }
    }
    return veh;
}

// ---------- remote peds (players on foot) ----------
static const char* g_pedPool[] = {
    "a_m_y_skater_01", "a_m_y_cyclist_01", "s_m_y_cop_01",
    "a_f_y_hipster_01", "a_m_y_dhill_01", "a_m_o_tramp_01",
    "a_f_m_salton_01", "s_m_m_security_01"};

uint32_t spawnRemotePed(const Json& e) {
    uint32_t poolIdx = e.has("i") ? ((uint32_t)e.get("i")->num) % 8 : 0;
    uint32_t model = requestModel(g_pedPool[poolIdx]);
    if (!model) return 0;
    float x = e.has("x") ? (float)e.get("x")->asNum() : 0.0f;
    float y = e.has("y") ? (float)e.get("y")->asNum() : 0.0f;
    float h = e.has("h") ? (float)e.get("h")->asNum() : 0.0f;
    uint32_t ped = invoke<uint32_t>(N_CREATE_PED, 26ULL, (uint64_t)model,
                                    x, y, 200.0f, h, 0, 0);
    if (!ped) return 0;
    invoke<void>(N_SET_ENTITY_AS_MISSION_ENTITY, (uint64_t)ped, 1, 1, 1);
    invoke<void>(N_SET_ENTITY_COLLISION, (uint64_t)ped, 0, 1);  // ghost
    return ped;
}

void deleteRemotePed(uint32_t id) {
    auto it = g_remotePeds.find(id);
    if (it == g_remotePeds.end()) return;
    uint32_t ped = it->second;
    invoke<void>(N_SET_ENTITY_AS_MISSION_ENTITY, (uint64_t)ped, 1, 1, 0);
    invoke<void>(N_DELETE_ENTITY, (uint64_t)&ped);
    g_remotePeds.erase(it);
    g_remotePedZs.erase(id);
    g_pedLerp.erase(id);
}

void deleteRemoteVehicle(uint32_t id) {
    auto it = g_remoteVehs.find(id);
    if (it == g_remoteVehs.end()) return;
    uint32_t veh = it->second;
    invoke<void>(N_SET_ENTITY_AS_MISSION_ENTITY, (uint64_t)veh, 1, 1, 0);
    invoke<void>(N_DELETE_ENTITY, (uint64_t)&veh);
    g_remoteVehs.erase(it);
    g_remoteZs.erase(id);
    g_vehLerp.erase(id);
}

uint32_t spawnRemoteObject(const Json& e) {
    std::string model = e.has("m") ? e.get("m")->asStr("prop_ld_conc_pipes02") : "prop_ld_conc_pipes02";
    uint32_t m = joaat(model);
    if (!m) return 0;
    invoke<void>(N_REQUEST_MODEL, (uint64_t)m);
    for (int i = 0; i < 200 && !invoke<bool>(N_HAS_MODEL_LOADED, (uint64_t)m); ++i)
        Sleep(20);
    if (!invoke<bool>(N_HAS_MODEL_LOADED, (uint64_t)m)) return 0;
    float x = e.has("x") ? (float)e.get("x")->asNum() : 0.0f;
    float y = e.has("y") ? (float)e.get("y")->asNum() : 0.0f;
    float z = e.has("z") ? (float)e.get("z")->asNum() : 0.0f;
    float h = e.has("h") ? (float)e.get("h")->asNum() : 0.0f;
    uint32_t obj = invoke<uint32_t>(N_CREATE_OBJECT, (uint64_t)m, x, y, z, 1, 0, 1);
    if (!obj) return 0;
    invoke<void>(N_SET_ENTITY_HEADING, (uint64_t)obj, h);
    invoke<void>(N_SET_ENTITY_AS_MISSION_ENTITY, (uint64_t)obj, 1, 1, 1);
    return obj;
}

void deleteRemoteObject(uint32_t id) {
    auto it = g_remoteObjs.find(id);
    if (it == g_remoteObjs.end()) return;
    uint32_t obj = it->second;
    if (invoke<bool>(N_DOES_ENTITY_EXIST, (uint64_t)obj)) {
        invoke<void>(N_SET_ENTITY_AS_MISSION_ENTITY, (uint64_t)obj, 1, 1, 0);
        invoke<void>(N_DELETE_ENTITY, (uint64_t)&obj);
    }
    g_remoteObjs.erase(it);
}

// ---------- chat display ----------
void showHelp(const std::string& text) {
    invoke<void>(N_BEGIN_TEXT_COMMAND_DISPLAY_HELP, (uint64_t)joaat("STRING"));
    invoke<void>(N_ADD_TEXT_COMPONENT_SUBSTRING_PLAYER_NAME, (uint64_t)text.c_str());
    invoke<void>(N_END_TEXT_COMMAND_DISPLAY_HELP, 0, 0, 1, 4000);
}

// ---------- protocol handling ----------
void handleMessage(const Json& m) {
    std::string t = m.asStr();
    if (t == "hello") {
        g_joined = true;
        g_ownVehSpawned = false;
        g_myId = m.has("id") ? (uint32_t)m.get("id")->num : 0;
        const Json* spawn = m.get("spawn");
        double x = spawn && spawn->arr.size() > 0 ? spawn->arr[0].num : 0;
        double y = spawn && spawn->arr.size() > 1 ? spawn->arr[1].num : 0;
        double h = spawn && spawn->arr.size() > 2 ? spawn->arr[2].num : 0;
        char buf[128];
        snprintf(buf, sizeof buf, "Connected to MyMP as %s (#%.0f). Spawning %s...",
                 g_cfg.name.c_str(), m.has("id") ? m.get("id")->num : 0.0, g_cfg.vehicle.c_str());
        logLine(buf);
        showHelp("Connected to MyMP! Spawning your vehicle...");
        g_ownVeh = spawnOwnVehicle(g_cfg.vehicle, (float)x, (float)y, 0.0f, (float)h);
        if (g_ownVeh) g_ownVehSpawned = true;
        logLine(g_ownVeh ? "Own vehicle spawned." : "WARNING: could not spawn vehicle — check model name in mymp.ini");
    } else if (t == "state") {
        const Json* ents = m.get("ents");
        if (!ents) return;
        std::map<uint32_t, bool> seen;
        for (const Json& e : ents->arr) {
            if (!e.has("i")) continue;
            uint32_t id = (uint32_t)e.get("i")->num;
            seen[id] = true;
            if (e.has("k") && e.get("k")->asStr() == "obj") {
                // custom map object (asset streaming lite)
                if (g_remoteObjs.find(id) == g_remoteObjs.end()) {
                    uint32_t obj = spawnRemoteObject(e);
                    if (obj) g_remoteObjs[id] = obj;
                }
                continue;
            }
            if (e.has("n")) g_remoteNames[id] = e.get("n")->asStr("?");
            if (e.has("hp")) g_remoteHp[id] = (int)e.get("hp")->num;
            int foot = e.has("f") ? (int)e.get("f")->num : 0;
            if (foot) {
                // player is on foot -> a ped
                if (g_remoteVehs.count(id)) deleteRemoteVehicle(id);
                if (g_remotePeds.find(id) == g_remotePeds.end()) {
                    uint32_t ped = spawnRemotePed(e);
                    if (ped) g_remotePeds[id] = ped;
                }
                auto it = g_remotePeds.find(id);
                if (it == g_remotePeds.end()) continue;
                uint32_t ped = it->second;
                float x = e.has("x") ? (float)e.get("x")->asNum() : 0.0f;
                float y = e.has("y") ? (float)e.get("y")->asNum() : 0.0f;
                float h = e.has("h") ? (float)e.get("h")->asNum() : 0.0f;
                float z = 200.0f;
                auto zit = g_remotePedZs.find(id);
                if (zit != g_remotePedZs.end()) z = zit->second;
                float spd = e.has("s") ? (float)e.get("s")->asNum() : 0.0f;
                auto lit = g_pedLerp.find(id);
                if (lit == g_pedLerp.end() || !lit->second.have) {
                    // first sighting: place instantly so the entity settles
                    invoke<void>(N_SET_ENTITY_COORDS, (uint64_t)ped, x, y, z, 0, 0, 0, 1);
                    invoke<void>(N_SET_ENTITY_HEADING, (uint64_t)ped, h);
                    invoke<void>(N_SET_ENTITY_VELOCITY, (uint64_t)ped, 0.0f, 0.0f, 0.0f);
                    if (z == 200.0f) {
                        Vector3* pos = invoke<Vector3*>(N_GET_ENTITY_COORDS, (uint64_t)ped, 0, 0);
                        if (pos) { z = pos->z; g_remotePedZs[id] = z; }
                    }
                    LerpState& ls = g_pedLerp[id];
                    ls.have = true; ls.x = x; ls.y = y; ls.z = z;
                    ls.h = h; ls.spd = spd; ls.t = GetTickCount64();
                } else {
                    LerpState& ls = lit->second;
                    if (z != 200.0f) ls.z = z;
                    ls.x = x; ls.y = y; ls.h = h; ls.spd = spd; ls.t = GetTickCount64();
                }
                if (e.has("hp"))
                    invoke<void>(N_SET_ENTITY_HEALTH, (uint64_t)ped,
                                 100 + (int)e.get("hp")->num, 0);
                if (e.has("ar"))
                    invoke<void>(N_SET_PED_ARMOUR, (uint64_t)ped, (int)e.get("ar")->num, 0);
            } else {
                // player is in a vehicle
                if (g_remotePeds.count(id)) deleteRemotePed(id);
                if (g_remoteVehs.find(id) == g_remoteVehs.end()) {
                    uint32_t veh = spawnRemoteVehicle(e);
                    if (veh) g_remoteVehs[id] = veh;
                }
                auto it = g_remoteVehs.find(id);
                if (it == g_remoteVehs.end()) continue;
                uint32_t veh = it->second;
                float x = e.has("x") ? (float)e.get("x")->asNum() : 0.0f;
                float y = e.has("y") ? (float)e.get("y")->asNum() : 0.0f;
                float h = e.has("h") ? (float)e.get("h")->asNum() : 0.0f;
                float z = 200.0f;
                auto zit = g_remoteZs.find(id);
                if (zit != g_remoteZs.end()) z = zit->second;
                float spd = e.has("s") ? (float)e.get("s")->asNum() : 0.0f;
                auto lit = g_vehLerp.find(id);
                if (lit == g_vehLerp.end() || !lit->second.have) {
                    // first sighting: place instantly so the car settles on terrain
                    invoke<void>(N_SET_ENTITY_COORDS, (uint64_t)veh, x, y, z, 0, 0, 0, 1);
                    invoke<void>(N_SET_ENTITY_HEADING, (uint64_t)veh, h);
                    if (spd > 0.1f)
                        invoke<void>(N_SET_VEHICLE_FORWARD_SPEED, (uint64_t)veh, spd);
                    if (z == 200.0f) {
                        Vector3* pos = invoke<Vector3*>(N_GET_ENTITY_COORDS, (uint64_t)veh, 0, 0);
                        if (pos) { z = pos->z; g_remoteZs[id] = z; }
                    }
                    LerpState& ls = g_vehLerp[id];
                    ls.have = true; ls.x = x; ls.y = y; ls.z = z;
                    ls.h = h; ls.spd = spd; ls.t = GetTickCount64();
                } else {
                    LerpState& ls = lit->second;
                    if (z != 200.0f) ls.z = z;
                    ls.x = x; ls.y = y; ls.h = h; ls.spd = spd; ls.t = GetTickCount64();
                }
            }
        }
        // remove entities that left the visible set
        for (auto it = g_remoteVehs.begin(); it != g_remoteVehs.end();) {
            if (seen.find(it->first) == seen.end()) {
                deleteRemoteVehicle(it->first);
                it = g_remoteVehs.begin();
            } else ++it;
        }
        for (auto it = g_remotePeds.begin(); it != g_remotePeds.end();) {
            if (seen.find(it->first) == seen.end()) {
                deleteRemotePed(it->first);
                it = g_remotePeds.begin();
            } else ++it;
        }
        for (auto it = g_remoteObjs.begin(); it != g_remoteObjs.end();) {
            if (seen.find(it->first) == seen.end()) {
                deleteRemoteObject(it->first);
                it = g_remoteObjs.begin();
            } else ++it;
        }
        for (auto it = g_remoteNames.begin(); it != g_remoteNames.end();) {
            if (seen.find(it->first) == seen.end()) {
                uint32_t gone = it->first;
                g_remoteNames.erase(it++);
                g_remoteHp.erase(gone);
            } else ++it;
        }
    } else if (t == "event") {
        std::string name = m.has("name") ? m.get("name")->asStr() : "";
        const Json* data = m.get("data");
        if (name == "spawnVehicle" || name == "setVehicle") {
            std::string model = data && data->has("model") ? data->get("model")->asStr("adder") : "adder";
            uint32_t h = joaat(model);
            if (h && invoke<bool>(N_IS_MODEL_A_VEHICLE, (uint64_t)h)) {
                uint32_t ped = invoke<uint32_t>(N_PLAYER_PED_ID);
                Vector3* pos = invoke<Vector3*>(N_GET_ENTITY_COORDS, (uint64_t)ped, 0, 0);
                uint32_t veh = spawnOwnVehicle(model, pos ? pos->x : 0.0f, pos ? pos->y : 0.0f,
                                               pos ? pos->z : 0.0f,
                                               invoke<float>(N_GET_ENTITY_HEADING, (uint64_t)ped));
                if (veh) {
                    if (g_ownVeh && g_ownVeh != veh) {
                        uint32_t old = g_ownVeh;
                        invoke<void>(N_DELETE_ENTITY, (uint64_t)&old);
                    }
                    g_ownVeh = veh;
                    g_ownVehSpawned = true;
                    showHelp("Spawned " + model + ".");
                } else showHelp("Could not spawn " + model + ".");
            } else showHelp("Unknown vehicle model: " + model + ".");
        } else if (name == "deleteVehicle") {
            if (g_ownVeh && invoke<bool>(N_DOES_ENTITY_EXIST, (uint64_t)g_ownVeh)) {
                uint32_t old = g_ownVeh;
                invoke<void>(N_DELETE_ENTITY, (uint64_t)&old);
            }
            g_ownVeh = 0;
            g_ownVehSpawned = false;
            showHelp("Vehicle deleted.");
        } else if (name == "giveWeapon") {
            std::string weapon = data && data->has("weapon")
                ? data->get("weapon")->asStr("WEAPON_PISTOL") : "WEAPON_PISTOL";
            uint32_t ped = invoke<uint32_t>(N_PLAYER_PED_ID);
            if (ped) {
                invoke<void>(N_GIVE_WEAPON_TO_PED, (uint64_t)ped, (uint64_t)joaat(weapon),
                             9999, 0, 1);
                showHelp("Gave you " + weapon + ".");
            }
        } else if (name == "damage") {
            int target = data && data->has("target") ? (int)data->get("target")->num : -1;
            std::string by = data && data->has("by") ? data->get("by")->asStr("?") : "?";
            int amount = data && data->has("amount") ? (int)data->get("amount")->num : 0;
            if (target == (int)g_myId) {
                std::string line = by + " hit you (-" + std::to_string(amount) + ")";
                g_chatLog.push_back(line);
                if (g_chatLog.size() > 6) g_chatLog.pop_front();
                showHelp(line);
            }
        } else if (name == "death") {
            int id = data && data->has("id") ? (int)data->get("id")->num : -1;
            std::string by = data && data->has("by") ? data->get("by")->asStr("?") : "?";
            std::string line = (id == (int)g_myId)
                ? "You died (killed by " + by + ")."
                : by + " got a kill.";
            g_chatLog.push_back(line);
            if (g_chatLog.size() > 6) g_chatLog.pop_front();
            showHelp(line);
        } else if (name == "respawn") {
            if (data && data->has("id") && (int)data->get("id")->num == (int)g_myId) {
                uint32_t ped = invoke<uint32_t>(N_PLAYER_PED_ID);
                if (ped) {
                    invoke<void>(N_RESURRECT_PED, (uint64_t)ped);
                    invoke<void>(N_SET_ENTITY_HEALTH, (uint64_t)ped, 200, 0);
                    float x = data->has("x") ? (float)data->get("x")->num : 0.0f;
                    float y = data->has("y") ? (float)data->get("y")->num : 0.0f;
                    Vector3* pos = invoke<Vector3*>(N_GET_ENTITY_COORDS, (uint64_t)ped, 0, 0);
                    float z = pos ? pos->z : 200.0f;
                    invoke<void>(N_SET_ENTITY_COORDS, (uint64_t)ped, x, y, z, 0, 0, 0, 1);
                    g_chatLog.push_back("You respawned.");
                    if (g_chatLog.size() > 6) g_chatLog.pop_front();
                }
            }
        } else if (name == "announce" || name == "pm") {
            std::string msg = data && data->has("msg") ? data->get("msg")->asStr() : "";
            if (!msg.empty()) showHelp(msg.substr(0, 90));
        }
    } else if (t == "sys" || t == "chat") {
        std::string msg = m.has("msg") ? m.get("msg")->asStr() : "";
        if (t == "chat" && m.has("name")) msg = m.get("name")->asStr() + ": " + msg;
        if (!msg.empty()) {
            showHelp(msg.substr(0, 90));
            logLine("[chat] " + msg);
            g_chatLog.push_back(msg);
            if (g_chatLog.size() > 6) g_chatLog.pop_front();
        }
    } else if (t == "leave") {
        if (m.has("id")) {
            uint32_t id = (uint32_t)m.get("id")->num;
            deleteRemoteVehicle(id);
            deleteRemotePed(id);
        }
    }
}

// ---------- in-game chat: input + on-screen log ----------
static int pollChatChar() {
    for (int c = 'A'; c <= 'Z'; c++) if (GetAsyncKeyState(c) & 1) {
        return (GetAsyncKeyState(VK_SHIFT) & 0x8000) ? c : (c - 'A' + 'a');
    }
    for (int c = '0'; c <= '9'; c++) if (GetAsyncKeyState(c) & 1) {
        if (GetAsyncKeyState(VK_SHIFT) & 0x8000) {
            static const char* shifted = ")!@#$%^&*(";
            return shifted[c - '0'];
        }
        return c;
    }
    if (GetAsyncKeyState(VK_SPACE) & 1) return ' ';
    struct { int vk; char plain, shifted; } punct[] = {
        {VK_OEM_COMMA, ',', '<'}, {VK_OEM_PERIOD, '.', '>'},
        {VK_OEM_1, ';', ':'},     {VK_OEM_7, '\'', '"'},
        {VK_OEM_2, '/', '?'},     {VK_OEM_MINUS, '-', '_'},
        {VK_OEM_PLUS, '=', '+'},  {VK_OEM_4, '[', '{'},
        {VK_OEM_6, ']', '}'},     {VK_OEM_5, '\\', '|'},
        {VK_OEM_3, '`', '~'},
    };
    bool shift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
    for (auto& pt : punct) if (GetAsyncKeyState(pt.vk) & 1) return shift ? pt.shifted : pt.plain;
    return 0;
}

static void drawTextLine(const std::string& text, float y) {
    invoke<void>(N_SET_TEXT_FONT, 0ULL);
    invoke<void>(N_SET_TEXT_SCALE, 0.5f, 0.5f);
    invoke<void>(N_SET_TEXT_COLOUR, 255, 255, 255, 255);
    invoke<void>(N_SET_TEXT_OUTLINE);
    invoke<void>(N_BEGIN_TEXT_COMMAND_DISPLAY_TEXT, (uint64_t)joaat("STRING"));
    invoke<void>(N_ADD_TEXT_COMPONENT_SUBSTRING_PLAYER_NAME, (uint64_t)text.c_str());
    invoke<void>(N_END_TEXT_COMMAND_DISPLAY_TEXT, 0.03f, y);
}

void pollChatInput() {
    if (!g_joined) return;
    if (!g_chatOpen && (GetAsyncKeyState('T') & 1)) {
        g_chatOpen = true;
        g_chatBuf.clear();
    }
    if (!g_chatOpen) return;
    // block driving keys while typing
    invoke<void>(N_DISABLE_CONTROL_ACTION, 0ULL, 32ULL, 1ULL);
    invoke<void>(N_DISABLE_CONTROL_ACTION, 0ULL, 33ULL, 1ULL);
    invoke<void>(N_DISABLE_CONTROL_ACTION, 0ULL, 34ULL, 1ULL);
    invoke<void>(N_DISABLE_CONTROL_ACTION, 0ULL, 35ULL, 1ULL);
    if (GetAsyncKeyState(VK_BACK) & 1) {
        if (!g_chatBuf.empty()) g_chatBuf.pop_back();
        return;
    }
    if (GetAsyncKeyState(VK_ESCAPE) & 1) { g_chatOpen = false; return; }
    if (GetAsyncKeyState(VK_RETURN) & 1) {
        if (!g_chatBuf.empty()) {
            g_sock.send(msgChat(g_chatBuf));
            std::string line = "You: " + g_chatBuf;
            g_chatLog.push_back(line);
            if (g_chatLog.size() > 6) g_chatLog.pop_front();
            logLine("[chat] " + line);
        }
        g_chatOpen = false;
        return;
    }
    int c = pollChatChar();
    if (c && (int)g_chatBuf.size() < 120) g_chatBuf.push_back((char)c);
}

void chatDisplay() {
    if (!g_joined || (g_chatLog.empty() && !g_chatOpen)) return;
    invoke<void>(N_DRAW_RECT, 0.18f, 0.88f, 0.36f, 0.26f, 0, 0, 0, 160);
    float y = 0.985f;
    std::string input = "> " + g_chatBuf + (g_chatOpen ? "_" : "");
    drawTextLine(input, y);
    y -= 0.034f;
    for (auto it = g_chatLog.rbegin(); it != g_chatLog.rend() && y > 0.62f; ++it, y -= 0.034f)
        drawTextLine(*it, y);
}

// ---------- player list overlay (P key) ----------
void pollPlayerList() {
    if (GetAsyncKeyState('P') & 1) g_playerList = !g_playerList;
}

void playerListDisplay() {
    if (!g_playerList || !g_joined) return;
    float n = (float)g_remoteNames.size();
    float h = 0.034f * n + 0.06f;
    invoke<void>(N_DRAW_RECT, 0.87f, 0.5f, 0.26f, h, 0, 0, 0, 170);
    float y = 0.5f - h / 2 + 0.035f;
    invoke<void>(N_SET_TEXT_FONT, 0ULL);
    invoke<void>(N_SET_TEXT_SCALE, 0.45f, 0.45f);
    invoke<void>(N_SET_TEXT_COLOUR, 255, 159, 28, 255);
    invoke<void>(N_SET_TEXT_OUTLINE);
    invoke<void>(N_BEGIN_TEXT_COMMAND_DISPLAY_TEXT, (uint64_t)joaat("STRING"));
    invoke<void>(N_ADD_TEXT_COMPONENT_SUBSTRING_PLAYER_NAME, (uint64_t)"Players");
    invoke<void>(N_END_TEXT_COMMAND_DISPLAY_TEXT, 0.76f, y);
    y += 0.04f;
    for (auto& kv : g_remoteNames) {
        int hp = g_remoteHp.count(kv.first) ? g_remoteHp[kv.first] : 100;
        std::string line = kv.second + "  [" + std::to_string(hp) + "%]";
        drawTextLine(line, y);
        y += 0.034f;
    }
}

// ---------- remote-entity interpolation (per tick) ----------
// GTA:Network (MIT) design: render between snapshots on a short delay, not by
// snapping to each 10 Hz update. Exponential chase toward the stored target +
// heading-based extrapolation while updates are late. Own implementation.
static float lerpAngleDeg(float a, float b, float k) {
    float d = fmodf(b - a, 360.0f);
    if (d > 180.0f) d -= 360.0f;
    if (d < -180.0f) d += 360.0f;
    return a + d * k;
}

void applyRemoteLerp() {
    uint64_t now = GetTickCount64();
    static uint64_t lastTick = 0;
    float dt = 0.016f;
    if (lastTick) {
        uint64_t ms = now - lastTick;
        if (ms > 0 && ms < 250) dt = (float)ms / 1000.0f;
    }
    lastTick = now;
    const float k = 1.0f - expf(-dt / 0.09f);
    const float TAU_DEG = 3.14159265f / 180.0f;

    for (auto& kv : g_remoteVehs) {
        auto it = g_vehLerp.find(kv.first);
        if (it == g_vehLerp.end() || !it->second.have) continue;
        LerpState& ls = it->second;
        uint32_t veh = kv.second;
        Vector3* pos = invoke<Vector3*>(N_GET_ENTITY_COORDS, (uint64_t)veh, 0, 0, 0);
        if (!pos) continue;
        float tx = ls.x, ty = ls.y;
        if (now - ls.t > 300 && ls.spd > 0.1f) {  // extrapolate while updates are late
            float ex = (float)(now - ls.t) / 1000.0f;
            tx += cosf(ls.h * TAU_DEG) * ls.spd * ex;
            ty += sinf(ls.h * TAU_DEG) * ls.spd * ex;
        }
        float nx = pos->x + (tx - pos->x) * k;
        float ny = pos->y + (ty - pos->y) * k;
        float nz = pos->z + (ls.z - pos->z) * k;
        if (fabsf(nx - pos->x) < 0.001f && fabsf(ny - pos->y) < 0.001f &&
            fabsf(nz - pos->z) < 0.001f) continue;
        invoke<void>(N_SET_ENTITY_COORDS, (uint64_t)veh, nx, ny, nz, 0, 0, 0, 1);
        invoke<void>(N_SET_ENTITY_HEADING, (uint64_t)veh,
                     lerpAngleDeg(invoke<float>(N_GET_ENTITY_HEADING, (uint64_t)veh), ls.h, k));
        if (ls.spd > 0.1f)
            invoke<void>(N_SET_VEHICLE_FORWARD_SPEED, (uint64_t)veh, ls.spd);
    }
    for (auto& kv : g_remotePeds) {
        auto it = g_pedLerp.find(kv.first);
        if (it == g_pedLerp.end() || !it->second.have) continue;
        LerpState& ls = it->second;
        uint32_t ped = kv.second;
        Vector3* pos = invoke<Vector3*>(N_GET_ENTITY_COORDS, (uint64_t)ped, 0, 0, 0);
        if (!pos) continue;
        float tx = ls.x, ty = ls.y;
        if (now - ls.t > 300 && ls.spd > 0.1f) {
            float ex = (float)(now - ls.t) / 1000.0f;
            tx += cosf(ls.h * TAU_DEG) * ls.spd * ex;
            ty += sinf(ls.h * TAU_DEG) * ls.spd * ex;
        }
        float nx = pos->x + (tx - pos->x) * k;
        float ny = pos->y + (ty - pos->y) * k;
        float nz = pos->z + (ls.z - pos->z) * k;
        if (fabsf(nx - pos->x) < 0.001f && fabsf(ny - pos->y) < 0.001f &&
            fabsf(nz - pos->z) < 0.001f) continue;
        invoke<void>(N_SET_ENTITY_COORDS, (uint64_t)ped, nx, ny, nz, 0, 0, 0, 1);
        invoke<void>(N_SET_ENTITY_HEADING, (uint64_t)ped,
                     lerpAngleDeg(invoke<float>(N_GET_ENTITY_HEADING, (uint64_t)ped), ls.h, k));
    }
}

// ---------- main client loop ----------
// One iteration of client work. Runs either from inside a GTA script thread
// (see scriptthread.cpp — needed for natives like REQUEST_MODEL to work)
// or, as a fallback, from a worker thread at 10 Hz.
void clientTick() {
    static uint64_t lastReport = 0;
    // --- drain incoming ---
    std::string data;
    while (g_sock.recv(data)) {
        Json m;
        if (m.parse(data)) handleMessage(m);
    }
    if (g_joined) {
        applyRemoteLerp();  // smooth remote entities every frame
        pollChatInput();
        chatDisplay();
        pollPlayerList();
        playerListDisplay();
    }
    // --- report own state at 10 Hz ---
    uint64_t now = GetTickCount64();
    if (g_joined && now - lastReport >= 100) {
        lastReport = now;
        uint32_t ped = invoke<uint32_t>(N_PLAYER_PED_ID);
        if (ped) {
            bool inVeh = invoke<bool>(N_IS_PED_IN_ANY_VEHICLE, (uint64_t)ped, 0);
            uint32_t veh = inVeh
                ? invoke<uint32_t>(N_GET_VEHICLE_PED_IS_IN, (uint64_t)ped, 0)
                : 0;
            if (!veh && g_ownVeh && invoke<bool>(N_DOES_ENTITY_EXIST, (uint64_t)g_ownVeh))
                veh = g_ownVeh;
            int hp = 100, ar = 0;
            if (ped) {
                int rawHp = invoke<int>(N_GET_ENTITY_HEALTH, (uint64_t)ped);
                hp = rawHp <= 0 ? 0 : (rawHp - 100);   // GTA peds: 100..200
                if (hp < 0) hp = 0; if (hp > 100) hp = 100;
                ar = invoke<int>(N_GET_PED_ARMOUR, (uint64_t)ped);
                if (ar < 0) ar = 0; if (ar > 100) ar = 100;
            }
            if (veh && invoke<bool>(N_DOES_ENTITY_EXIST, (uint64_t)veh)) {
                Vector3* pos = invoke<Vector3*>(N_GET_ENTITY_COORDS, (uint64_t)veh, 0, 0, 0);
                float h = invoke<float>(N_GET_ENTITY_HEADING, (uint64_t)veh);
                float s = invoke<float>(N_GET_ENTITY_SPEED, (uint64_t)veh);
                uint32_t model = invoke<uint32_t>(N_GET_ENTITY_MODEL, (uint64_t)veh);
                if (pos && (s > 0.1f || now - lastReport >= 1000))
                    g_sock.send(msgState(pos->x, pos->y, h, s, model, 0, hp, ar));
            } else {
                // on foot: report the ped itself
                Vector3* pos = invoke<Vector3*>(N_GET_ENTITY_COORDS, (uint64_t)ped, 0, 0, 0);
                float h = invoke<float>(N_GET_ENTITY_HEADING, (uint64_t)ped);
                float s = invoke<float>(N_GET_ENTITY_SPEED, (uint64_t)ped);
                if (pos && (s > 0.1f || now - lastReport >= 1000))
                    g_sock.send(msgState(pos->x, pos->y, h, s, 0, 1, hp, ar));
            }
        }
    }
    // --- combat: report damage we dealt to remote entities (GTAMP-style:
    //     shooter reports, server routes + applies) ---
    static uint64_t lastDmg = 0;
    if (g_joined && now - lastDmg >= 300) {
        uint32_t myPed = invoke<uint32_t>(N_PLAYER_PED_ID);
        if (myPed) {
            bool hit = false;
            for (auto& kv : g_remoteVehs) {
                if (invoke<bool>(N_HAS_ENTITY_BEEN_DAMAGED_BY_ENTITY,
                                 (uint64_t)kv.second, (uint64_t)myPed)) {
                    g_sock.send("{\"t\":\"damage\",\"target\":" +
                                std::to_string(kv.first) + ",\"amount\":25}");
                    invoke<void>(N_CLEAR_ENTITY_LAST_DAMAGE_ENTITY, (uint64_t)kv.second);
                    hit = true;
                    break;
                }
            }
            if (!hit) {
                for (auto& kv : g_remotePeds) {
                    if (invoke<bool>(N_HAS_ENTITY_BEEN_DAMAGED_BY_ENTITY,
                                     (uint64_t)kv.second, (uint64_t)myPed)) {
                        g_sock.send("{\"t\":\"damage\",\"target\":" +
                                    std::to_string(kv.first) + ",\"amount\":25}");
                        invoke<void>(N_CLEAR_ENTITY_LAST_DAMAGE_ENTITY, (uint64_t)kv.second);
                        hit = true;
                        break;
                    }
                }
            }
            if (hit) lastDmg = now;
        }
    }
}

void clientLoop() {
    // wait for the game to be fully up
    for (int i = 0; i < 60 && !findNativeTable(); ++i) Sleep(2000);
    uint8_t* tbl = findNativeTable();
    if (!tbl) {
        logLine("FATAL: could not find GTA V native table. Is GTA5.exe running? "
                "Does this build need a pattern update?");
        return;
    }
    memcpy(g_nativeTable, tbl, sizeof g_nativeTable);
    logLine("Native table discovered.");

    // sanity call: PLAYER_PED_ID should return a handle or 0
    uint32_t ped = invoke<uint32_t>(N_PLAYER_PED_ID);
    logLine(ped ? "Game ready — PLAYER_PED_ID OK." : "Game ready (PLAYER_PED_ID=0).");

    if (!g_sock.open(g_cfg.host, g_cfg.port)) {
        logLine("FATAL: could not open UDP socket to " + g_cfg.host + ":" + std::to_string(g_cfg.port));
        return;
    }
    g_sock.send(msgJoin());
    logLine("Joining " + g_cfg.host + ":" + std::to_string(g_cfg.port) + " as " + g_cfg.name);

    // run inside a GTA script thread when possible — several natives
    // (REQUEST_MODEL, CREATE_VEHICLE…) need the game's script context
    if (mymp::installScriptTick(clientTick)) {
        logLine("Script-thread hook installed — natives run in game script context.");
        while (!g_quit) Sleep(100);
    } else {
        logLine("Script hook unavailable — falling back to worker thread "
                "(model loading may fail on some builds; check scriptthread.cpp).");
        while (!g_quit) {
            clientTick();
            Sleep(100);
        }
    }
    g_sock.close();
    logLine("Client stopped.");
}

}  // namespace

// ==================== DLL entry ====================
extern "C" BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(inst);
        // find our own directory (the game folder, since ASI loader loads from there)
        char path[MAX_PATH];
        GetModuleFileNameA(inst, path, MAX_PATH);
        std::string dir(path);
        size_t slash = dir.find_last_of('\\');
        g_gameDir = (slash == std::string::npos) ? "" : dir.substr(0, slash + 1);
        g_logPath = g_gameDir + "mymp.log";
        logLine("MyMP client loaded from " + g_gameDir);
        loadConfig();
        HANDLE th = CreateThread(nullptr, 0, [](LPVOID) -> DWORD {
            Sleep(5000);  // let the game boot first
            clientLoop();
            return 0;
        }, nullptr, 0, nullptr);
        if (th) CloseHandle(th);
    } else if (reason == DLL_PROCESS_DETACH) {
        g_quit = true;
    }
    return TRUE;
}
