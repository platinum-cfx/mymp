/*
 * mymp_launcher.c — MyMP Launcher (Windows x64 GUI).
 *
 * The alt:V / FiveM-style launcher exe: a window with a server browser
 * (queries a MyMP master list), connect settings, and one-click GTA V
 * launch. Installs the client files into your GTA folder on first run,
 * writes mymp.ini, then starts GTA V (via steam -applaunch when Steam
 * is detected). Original code written for MyMP.
 *
 * Build: zig cc -target x86_64-windows-gnu -O2 -mwindows \
 *            -lwinhttp -lws2_32 -ladvapi32 mymp_launcher.c -o MyMP-Launcher.exe
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winhttp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define ID_LIST    1001
#define ID_MASTER  1002
#define ID_REFRESH 1003
#define ID_HOST    1004
#define ID_PORT    1005
#define ID_NAME    1006
#define ID_VEH     1007
#define ID_COLOR   1008
#define ID_LAUNCH  1009
#define ID_STATUS  1010
#define ID_COPY    1011

#define MAX_SERVERS 64

typedef struct { char hostname[64]; char ip[64]; int port; int players; int maxclients; } Server;
static Server g_srv[MAX_SERVERS];
static int g_nsrv = 0;
static HWND hList, hMaster, hHost, hPort, hName, hVeh, hColor, hStatus;
static char g_exeDir[1024];

/* ---------------- tiny JSON extractor for /list ---------------- */
static void parseServers(const char* body) {
    g_nsrv = 0;
    const char* p = body;
    Server cur; int inObj = 0;
    memset(&cur, 0, sizeof cur);
    while (*p && g_nsrv < MAX_SERVERS) {
        if (*p == '{') { memset(&cur, 0, sizeof cur); inObj = 1; p++; continue; }
        if (*p == '}' && inObj) {
            if (cur.hostname[0] && cur.ip[0]) g_srv[g_nsrv++] = cur;
            inObj = 0; p++; continue;
        }
        if (*p == '"') {
            p++;
            char key[32]; int ki = 0;
            while (*p && *p != '"' && ki < 31) key[ki++] = *p++;
            key[ki] = 0; if (*p) p++;
            while (*p && (*p == ' ' || *p == ':')) p++;
            if (*p == '"') {
                p++;
                char val[96]; int vi = 0;
                while (*p && *p != '"' && vi < 95) val[vi++] = *p++;
                val[vi] = 0; if (*p) p++;
                if (!strcmp(key, "hostname")) strncpy(cur.hostname, val, 63);
                else if (!strcmp(key, "ip")) strncpy(cur.ip, val, 63);
            } else {
                char val[32]; int vi = 0;
                while (*p && *p != ',' && *p != '}' && vi < 31) val[vi++] = *p++;
                val[vi] = 0;
                if (!strcmp(key, "port")) cur.port = atoi(val);
                else if (!strcmp(key, "players")) cur.players = atoi(val);
                else if (!strcmp(key, "maxclients")) cur.maxclients = atoi(val);
            }
            continue;
        }
        p++;
    }
}

/* ---------------- HTTP GET (WinHTTP) ---------------- */
static char* httpGet(const char* url, int* ok) {
    *ok = 0;
    const char* hp = strstr(url, "://");
    if (!hp) return NULL;
    hp += 3;
    char h[256] = {0}, pth[512] = "/";
    int i = 0, port = 80;
    while (hp[i] && hp[i] != ':' && hp[i] != '/') { if (i < 255) h[i] = hp[i]; i++; }
    const char* pp = hp + i;
    if (*pp == ':') { port = atoi(pp + 1); pp = strchr(pp + 1, '/'); if (!pp) pp = ""; }
    if (*pp == '/') strncpy(pth, pp, 511);
    WCHAR wh[256], wpath[512];
    MultiByteToWideChar(CP_ACP, 0, h, -1, wh, 256);
    MultiByteToWideChar(CP_ACP, 0, pth, -1, wpath, 512);
    HINTERNET sess = WinHttpOpen(L"MyMP-Launcher/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, NULL, NULL, 0);
    if (!sess) return NULL;
    HINTERNET conn = WinHttpConnect(sess, wh, (INTERNET_PORT)port, 0);
    if (!conn) { WinHttpCloseHandle(sess); return NULL; }
    HINTERNET req = WinHttpOpenRequest(conn, L"GET", wpath, NULL, NULL, NULL, WINHTTP_FLAG_REFRESH);
    if (!req) { WinHttpCloseHandle(conn); WinHttpCloseHandle(sess); return NULL; }
    if (!WinHttpSendRequest(req, NULL, 0, NULL, 0, 0, 0)) { WinHttpCloseHandle(req); WinHttpCloseHandle(conn); WinHttpCloseHandle(sess); return NULL; }
    if (!WinHttpReceiveResponse(req, NULL)) { WinHttpCloseHandle(req); WinHttpCloseHandle(conn); WinHttpCloseHandle(sess); return NULL; }
    DWORD avail = 0, total = 0, cap = 8192;
    char* buf = (char*)malloc(cap);
    while (WinHttpQueryDataAvailable(req, &avail) && avail > 0) {
        if (total + avail + 1 > cap) { cap = (total + avail + 1) * 2; buf = (char*)realloc(buf, cap); }
        DWORD read = 0;
        if (!WinHttpReadData(req, buf + total, avail, &read)) break;
        total += read;
    }
    if (total) { buf[total] = 0; *ok = 1; }
    else { free(buf); buf = NULL; }
    WinHttpCloseHandle(req); WinHttpCloseHandle(conn); WinHttpCloseHandle(sess);
    return buf;
}

/* ---------------- helpers ---------------- */
static void statusf(const char* fmt, ...) {
    char tmp[512]; va_list ap; va_start(ap, fmt); vsnprintf(tmp, sizeof tmp, fmt, ap); va_end(ap);
    char old[2048];
    GetWindowTextA(hStatus, old, sizeof old);
    char both[2600];
    _snprintf(both, sizeof both, "%s%s%s", old, old[0] ? "\r\n" : "", tmp);
    SetWindowTextA(hStatus, both);
}
static void strip(char* s) { size_t n = strlen(s); while (n && (s[n-1] == '\n' || s[n-1] == '\r')) s[--n] = 0; }
static int fileExists(const char* p) {
    DWORD a = GetFileAttributesA(p);
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}
static void pathJoin(char* out, int n, const char* a, const char* b) { _snprintf(out, n, "%s\\%s", a, b); }

static int findGta(char* out, int n) {
    char buf[2048]; DWORD sz; HKEY k;
    buf[0] = 0; sz = sizeof buf;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Valve\\Steam", 0, KEY_READ, &k) == ERROR_SUCCESS) {
        if (RegQueryValueExA(k, "SteamPath", NULL, NULL, (BYTE*)buf, &sz) == ERROR_SUCCESS) strip(buf);
        RegCloseKey(k);
    }
    if (buf[0]) {
        pathJoin(out, n, buf, "steamapps\\common\\Grand Theft Auto V");
        if (fileExists(out)) return 1;
    }
    const char* regs[] = { "SOFTWARE\\WOW6432Node\\Rockstar Games\\Grand Theft Auto V",
                           "SOFTWARE\\Rockstar Games\\Grand Theft Auto V",
                           "Software\\Rockstar Games\\Grand Theft Auto V" };
    HKEY roots[] = { HKEY_LOCAL_MACHINE, HKEY_LOCAL_MACHINE, HKEY_CURRENT_USER };
    for (int i = 0; i < 3; i++) {
        if (RegOpenKeyExA(roots[i], regs[i], 0, KEY_READ, &k) != ERROR_SUCCESS) continue;
        buf[0] = 0; sz = sizeof buf;
        if (RegQueryValueExA(k, "InstallFolder", NULL, NULL, (BYTE*)buf, &sz) == ERROR_SUCCESS) strip(buf);
        RegCloseKey(k);
        if (buf[0] && fileExists(buf)) { strncpy(out, buf, n - 1); out[n-1] = 0; return 1; }
    }
    const char* tries[] = { "C:\\Program Files\\Epic Games\\GTAV",
                            "D:\\SteamLibrary\\steamapps\\common\\Grand Theft Auto V",
                            "E:\\SteamLibrary\\steamapps\\common\\Grand Theft Auto V" };
    for (int i = 0; i < 3; i++) if (fileExists(tries[i])) { strncpy(out, tries[i], n - 1); out[n-1] = 0; return 1; }
    return 0;
}

static void copyIfExists(const char* src, const char* dst) {
    if (!fileExists(src)) { statusf("  !! missing next to launcher: %s", src); return; }
    if (CopyFileA(src, dst, FALSE)) statusf("  + %s", dst);
    else statusf("  !! copy failed (%lu)", GetLastError());
}

/* ---- embedded payload: MyMP.asi + dinput8.dll appended to this exe ----
   MyMP.exe is a single self-extracting file (FiveM.exe-style): it carries the
   client + ASI loader inside itself and extracts them into the GTA folder. */
typedef struct { char magic[8]; uint64_t asioff, asilen, dlloff, dlllen; } PayloadHdr;

static int loadPayloadHdr(PayloadHdr* h) {
    char path[MAX_PATH]; DWORD n = GetModuleFileNameA(NULL, path, sizeof path);
    if (!n) return 0;
    FILE* f = fopen(path, "rb"); if (!f) return 0;
    if (fseek(f, -(long)sizeof(PayloadHdr), SEEK_END)) { fclose(f); return 0; }
    PayloadHdr t; if (fread(&t, 1, sizeof t, f) != sizeof t) { fclose(f); return 0; }
    fclose(f);
    if (memcmp(t.magic, "MYMPXSE1", 8)) return 0;
    *h = t; return 1;
}

static int extractTo(const char* dst, uint64_t off, uint64_t len) {
    char path[MAX_PATH]; DWORD n = GetModuleFileNameA(NULL, path, sizeof path);
    if (!n) return 0;
    FILE* in = fopen(path, "rb"); if (!in) return 0;
    FILE* out = fopen(dst, "wb"); if (!out) { fclose(in); return 0; }
    if (fseek(in, (long)off, SEEK_SET)) { fclose(in); fclose(out); return 0; }
    char buf[65536]; uint64_t left = len; int ok = 1;
    while (left) {
        size_t want = left > sizeof buf ? sizeof buf : (size_t)left;
        size_t r = fread(buf, 1, want, in);
        if (r != want) { ok = 0; break; }
        if (fwrite(buf, 1, r, out) != r) { ok = 0; break; }
        left -= r;
    }
    fclose(in); fclose(out);
    if (!ok) DeleteFileA(dst);
    return ok;
}

static void installClient(const char* gta) {
    PayloadHdr h;
    char dst[2048];
    if (loadPayloadHdr(&h)) {
        pathJoin(dst, sizeof dst, gta, "MyMP.asi");
        if (extractTo(dst, h.asioff, h.asilen)) statusf("  + MyMP.asi (from MyMP.exe)");
        else statusf("  !! extract MyMP.asi failed");
        pathJoin(dst, sizeof dst, gta, "dinput8.dll");
        if (extractTo(dst, h.dlloff, h.dlllen)) statusf("  + dinput8.dll (from MyMP.exe)");
        else statusf("  !! extract dinput8.dll failed");
        return;
    }
    /* dev fallback: files beside this exe */
    char src[2048];
    pathJoin(src, sizeof src, g_exeDir, "MyMP.asi");   pathJoin(dst, sizeof dst, gta, "MyMP.asi");
    copyIfExists(src, dst);
    pathJoin(src, sizeof src, g_exeDir, "dinput8.dll"); pathJoin(dst, sizeof dst, gta, "dinput8.dll");
    copyIfExists(src, dst);
}

static void writeIni(const char* gta) {
    char dst[2048], host[128], port[16], name[64], veh[64], col[16];
    GetDlgItemTextA(GetParent(hHost), ID_HOST, host, sizeof host);
    GetDlgItemTextA(GetParent(hHost), ID_PORT, port, sizeof port);
    GetDlgItemTextA(GetParent(hHost), ID_NAME, name, sizeof name);
    GetDlgItemTextA(GetParent(hHost), ID_VEH, veh, sizeof veh);
    GetDlgItemTextA(GetParent(hHost), ID_COLOR, col, sizeof col);
    if (!host[0]) strcpy(host, "127.0.0.1");
    if (!port[0]) strcpy(port, "30120");
    if (!name[0]) strcpy(name, "GTA-Player");
    if (!veh[0])  strcpy(veh, "adder");
    if (!col[0])  strcpy(col, "#ff9f1c");
    pathJoin(dst, sizeof dst, gta, "mymp.ini");
    FILE* f = fopen(dst, "w");
    if (!f) { statusf("!! could not write %s", dst); return; }
    fprintf(f, "; mymp.ini — written by MyMP-Launcher.exe\n\n[server]\nhost=%s\nport=%s\n\n[player]\nname=%s\nvehicle=%s\ncolor=%s\n",
            host, port, name, veh, col);
    fclose(f);
    statusf("  + %s", dst);
}

static void launchGta(const char* gta) {
    char steamExe[2048]; HKEY k; DWORD sz = sizeof steamExe; steamExe[0] = 0;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Valve\\Steam", 0, KEY_READ, &k) == ERROR_SUCCESS) {
        if (RegQueryValueExA(k, "SteamPath", NULL, NULL, (BYTE*)steamExe, &sz) == ERROR_SUCCESS) {
            strip(steamExe);
            char tmp[2048]; pathJoin(tmp, sizeof tmp, steamExe, "steam.exe");
            if (!fileExists(tmp)) steamExe[0] = 0;
        } else steamExe[0] = 0;
        RegCloseKey(k);
    }
    STARTUPINFOA si = { sizeof si }; PROCESS_INFORMATION pi = { 0 };
    char cmd[2048]; BOOL ok;
    if (steamExe[0]) {
        _snprintf(cmd, sizeof cmd, "\"%s\" -applaunch 271590", steamExe);
        ok = CreateProcessA(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
    } else {
        pathJoin(cmd, sizeof cmd, gta, "GTA5.exe");
        ok = CreateProcessA(NULL, cmd, NULL, NULL, FALSE, 0, NULL, gta, &si, &pi);
    }
    if (ok) { CloseHandle(pi.hThread); CloseHandle(pi.hProcess); statusf("GTA V launching — MyMP client will connect."); }
    else statusf("!! could not launch GTA V (%lu) — start it manually.", GetLastError());
}

/* ---------------- window ---------------- */
static void addLabel(HWND parent, const char* text, int x, int y, int w) {
    CreateWindowExA(0, "STATIC", text, WS_CHILD | WS_VISIBLE, x, y, w, 20, parent, NULL, NULL, NULL);
}
static HWND addEdit(HWND parent, int id, int x, int y, int w, const char* def) {
    HWND h = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", def, WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                             x, y, w, 22, parent, (HMENU)(INT_PTR)id, NULL, NULL);
    SendMessageA(h, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);
    return h;
}

LRESULT CALLBACK WndProc(HWND hw, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: {
        CreateWindowExA(0, "STATIC", "MyMP Launcher — your own GTA V multiplayer",
                        WS_CHILD | WS_VISIBLE, 16, 10, 500, 22, hw, NULL, NULL, NULL);
        addLabel(hw, "Server browser (master list):", 16, 40, 260);
        hMaster = addEdit(hw, ID_MASTER, 16, 60, 360, "http://localhost:30130/list");
        CreateWindowExA(0, "BUTTON", "Refresh", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                        386, 60, 90, 24, hw, (HMENU)(INT_PTR)ID_REFRESH, NULL, NULL);
        hList = CreateWindowExA(WS_EX_CLIENTEDGE, "LISTBOX", "", WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT,
                                16, 88, 460, 190, hw, (HMENU)(INT_PTR)ID_LIST, NULL, NULL);
        SendMessageA(hList, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);
        addLabel(hw, "Connect:", 16, 292, 200);
        addLabel(hw, "Server IP:", 16, 316, 70);  hHost  = addEdit(hw, ID_HOST,  90, 314, 170, "127.0.0.1");
        addLabel(hw, "Port:", 272, 316, 40);      hPort  = addEdit(hw, ID_PORT, 312, 314, 60,  "30120");
        addLabel(hw, "Name:", 16, 344, 70);       hName  = addEdit(hw, ID_NAME,  90, 342, 170, "GTA-Player");
        addLabel(hw, "Vehicle:", 272, 344, 60);   hVeh   = addEdit(hw, ID_VEH,  332, 342, 100, "adder");
        addLabel(hw, "Colour #RRGGBB:", 16, 372, 120); hColor = addEdit(hw, ID_COLOR, 136, 370, 110, "#ff9f1c");
        CreateWindowExA(0, "BUTTON", "Launch GTA V (install + connect)", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                        16, 406, 240, 34, hw, (HMENU)(INT_PTR)ID_LAUNCH, NULL, NULL);
        CreateWindowExA(0, "BUTTON", "Install client files only", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                        266, 406, 210, 34, hw, (HMENU)(INT_PTR)ID_COPY, NULL, NULL);
        hStatus = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "Ready. Pick a server (Refresh) or type one, then Launch.",
                                  WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | WS_VSCROLL,
                                  16, 452, 460, 96, hw, (HMENU)(INT_PTR)ID_STATUS, NULL, NULL);
        SendMessageA(hStatus, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);
        return 0;
    }
    case WM_COMMAND: {
        int id = LOWORD(wp), code = HIWORD(wp);
        if (id == ID_REFRESH) {
            char url[512];
            GetDlgItemTextA(hw, ID_MASTER, url, sizeof url);
            statusf("Refreshing %s ...", url);
            int ok = 0;
            char* body = httpGet(url, &ok);
            if (!ok) { statusf("!! could not reach master list."); if (body) free(body); break; }
            parseServers(body);
            free(body);
            SendMessageA(hList, LB_RESETCONTENT, 0, 0);
            for (int i = 0; i < g_nsrv; i++) {
                char line[256];
                _snprintf(line, sizeof line, "%s  [%s:%d]  %d/%d players", g_srv[i].hostname,
                          g_srv[i].ip, g_srv[i].port, g_srv[i].players, g_srv[i].maxclients);
                SendMessageA(hList, LB_ADDSTRING, 0, (LPARAM)line);
            }
            statusf("Found %d server(s). Select one, then Launch.", g_nsrv);
        } else if (id == ID_LIST && code == LBN_SELCHANGE) {
            int sel = (int)SendMessageA(hList, LB_GETCURSEL, 0, 0);
            if (sel >= 0 && sel < g_nsrv) {
                SetDlgItemTextA(hw, ID_HOST, g_srv[sel].ip);
                char p[16]; _snprintf(p, sizeof p, "%d", g_srv[sel].port);
                SetDlgItemTextA(hw, ID_PORT, p);
            }
        } else if (id == ID_LAUNCH) {
            char gta[1024];
            if (!findGta(gta, sizeof gta)) {
                MessageBoxA(hw, "Could not find your GTA V install.\nRun MyMP-Setup.exe, or install manually (see TESTING.md).",
                            "MyMP Launcher", MB_OK | MB_ICONINFORMATION);
                break;
            }
            statusf("GTA V found: %s", gta);
            installClient(gta);
            writeIni(gta);
            launchGta(gta);
        } else if (id == ID_COPY) {
            char gta[1024];
            if (!findGta(gta, sizeof gta)) {
                MessageBoxA(hw, "Could not find your GTA V install.", "MyMP Launcher", MB_OK | MB_ICONINFORMATION);
                break;
            }
            installClient(gta);
            statusf("Client files installed into %s", gta);
        }
        return 0;
    }
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(hw, msg, wp, lp);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR cmd, int show) {
    DWORD n = GetModuleFileNameA(NULL, g_exeDir, sizeof g_exeDir);
    for (DWORD i = n; i > 0; i--) if (g_exeDir[i-1] == '\\') { g_exeDir[i-1] = 0; break; }
    WNDCLASSA wc = { 0 };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = "MyMPLauncher";
    RegisterClassA(&wc);
    HWND hw = CreateWindowExA(0, "MyMPLauncher", "MyMP Launcher", WS_OVERLAPPEDWINDOW,
                              CW_USEDEFAULT, CW_USEDEFAULT, 520, 610,
                              NULL, NULL, hInst, NULL);
    if (!hw) return 1;
    ShowWindow(hw, SW_SHOW);
    MSG msg;
    while (GetMessageA(&msg, NULL, 0, 0)) {
        if (!IsDialogMessageA(hw, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
    }
    return 0;
}
