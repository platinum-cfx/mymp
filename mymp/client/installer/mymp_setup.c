/*
 * MyMP-Setup.c — MyMP GTA V client installer + launcher (Windows x64).
 *
 * One-click install, mirroring how FiveM / GTAMP installers behave:
 *   1. locate your GTA V install (Steam registry, Rockstar Launcher registry),
 *   2. copy MyMP.asi + dinput8.dll (Ultimate ASI Loader) + mymp.ini into it,
 *   3. configure server/name/vehicle (writes mymp.ini),
 *   4. launch GTA V.
 *
 * Original code written for MyMP; cross-compiled with Zig (no toolchain needed).
 * Build:  zig cc -target x86_64-windows-gnu -O ReleaseFast -ladvapi32 \
 *             mymp_setup.c -o MyMP-Setup.exe
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string.h>

static void strip(char* s) {
    size_t n = strlen(s);
    while (n > 0 && (s[n-1] == '\n' || s[n-1] == '\r' || s[n-1] == ' ')) s[--n] = 0;
}
static int readLine(char* buf, int n) {
    if (!fgets(buf, n, stdin)) return 0;
    strip(buf);
    return 1;
}
static int fileExists(const char* p) {
    DWORD a = GetFileAttributesA(p);
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}
static void pathJoin(char* out, int n, const char* a, const char* b) {
    _snprintf(out, n, "%s\\%s", a, b);
}

/* locate GTA V install folder; returns 1 and fills out[] on success */
static int findGta(char* out, int n) {
    char buf[2048]; DWORD sz; HKEY k;
    /* 1) Steam: HKCU\Software\Valve\Steam\SteamPath -> steamapps\common\Grand Theft Auto V */
    buf[0] = 0; sz = sizeof buf;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Valve\\Steam", 0, KEY_READ, &k) == ERROR_SUCCESS) {
        if (RegQueryValueExA(k, "SteamPath", NULL, NULL, (BYTE*)buf, &sz) == ERROR_SUCCESS) strip(buf);
        RegCloseKey(k);
    }
    if (buf[0]) {
        pathJoin(out, n, buf, "steamapps\\common\\Grand Theft Auto V");
        if (fileExists(out)) return 1;
    }
    /* 2) Rockstar Games Launcher: HKLM\...\Rockstar Games\Grand Theft Auto V\InstallFolder */
    const char* regs[] = {
        "SOFTWARE\\WOW6432Node\\Rockstar Games\\Grand Theft Auto V",
        "SOFTWARE\\Rockstar Games\\Grand Theft Auto V",
        "Software\\Rockstar Games\\Grand Theft Auto V",
    };
    HKEY root[] = { HKEY_LOCAL_MACHINE, HKEY_LOCAL_MACHINE, HKEY_CURRENT_USER };
    for (int i = 0; i < 3; i++) {
        if (RegOpenKeyExA(root[i], regs[i], 0, KEY_READ, &k) != ERROR_SUCCESS) continue;
        buf[0] = 0; sz = sizeof buf;
        if (RegQueryValueExA(k, "InstallFolder", NULL, NULL, (BYTE*)buf, &sz) == ERROR_SUCCESS) strip(buf);
        RegCloseKey(k);
        if (buf[0] && fileExists(buf)) { strncpy(out, buf, n - 1); out[n-1] = 0; return 1; }
    }
    /* 3) common fallbacks */
    const char* tries[] = {
        "C:\\Program Files\\Epic Games\\GTAV",
        "D:\\SteamLibrary\\steamapps\\common\\Grand Theft Auto V",
        "E:\\SteamLibrary\\steamapps\\common\\Grand Theft Auto V",
    };
    for (int i = 0; i < 3; i++) if (fileExists(tries[i])) { strncpy(out, tries[i], n - 1); out[n-1] = 0; return 1; }
    return 0;
}

static int copyIfExists(const char* src, const char* dst) {
    if (!fileExists(src)) { printf("  !! missing beside this exe: %s\n", src); return 0; }
    if (CopyFileA(src, dst, FALSE)) { printf("  + %s\n", dst); return 1; }
    printf("  !! could not copy %s (error %lu)\n", src, GetLastError());
    return 0;
}

int main(void) {
    char exeDir[1024], src[2048], gta[2048], dst[2048], tmp[512];
    DWORD n = GetModuleFileNameA(NULL, exeDir, sizeof exeDir);
    /* exeDir currently holds exe path; cut to directory */
    for (DWORD i = n; i > 0; i--) if (exeDir[i-1] == '\\') { exeDir[i-1] = 0; break; }

    printf("=====================================================\n");
    printf("  MyMP — your own GTA V multiplayer\n");
    printf("  Installer + launcher (FiveM-style)\n");
    printf("=====================================================\n\n");

    /* 1) find GTA V */
    if (!findGta(gta, sizeof gta)) {
        printf("Could not find your GTA V install automatically.\n");
        printf("Enter your GTA V folder (where GTA5.exe lives):\n> ");
        if (!readLine(gta, sizeof gta)) return 1;
        pathJoin(tmp, sizeof tmp, gta, "GTA5.exe");
        if (!fileExists(tmp)) { printf("No GTA5.exe in that folder. Aborting.\n"); return 1; }
    }
    pathJoin(tmp, sizeof tmp, gta, "GTA5.exe");
    if (!fileExists(tmp)) {
        printf("Found the folder but no GTA5.exe: %s\n", gta);
        printf("Note: MyMP currently targets the classic GTA V (Legacy) build.\n");
        return 1;
    }
    printf("GTA V found: %s\n\n", gta);

    /* 2) copy the client files */
    printf("Installing into your GTA V folder...\n");
    pathJoin(src, sizeof src, exeDir, "MyMP.asi");   pathJoin(dst, sizeof dst, gta, "MyMP.asi");
    copyIfExists(src, dst);
    pathJoin(src, sizeof src, exeDir, "dinput8.dll"); pathJoin(dst, sizeof dst, gta, "dinput8.dll");
    copyIfExists(src, dst);

    /* 3) configure */
    pathJoin(dst, sizeof dst, gta, "mymp.ini");
    char host[128] = "127.0.0.1", port[16] = "30120",
         name[64] = "GTA-Player", veh[64] = "adder", col[16] = "#ff9f1c";
    if (fileExists(dst)) {
        printf("\nA mymp.ini already exists in your GTA folder — keep it? [Y/n] > ");
        if (readLine(tmp, sizeof tmp) && (tmp[0] == 'n' || tmp[0] == 'N')) fileExists("x"); /* fallthrough: overwrite */
        else { printf("Keeping existing mymp.ini. Launching...\n\n"); goto launch; }
    }
    printf("\nConfigure (Enter keeps the default):\n");
    printf("  Server IP [%s] > ", host);   if (readLine(tmp, sizeof tmp) && tmp[0]) strncpy(host, tmp, sizeof host - 1);
    printf("  Server port [%s] > ", port); if (readLine(tmp, sizeof tmp) && tmp[0]) strncpy(port, tmp, sizeof port - 1);
    printf("  Your name [%s] > ", name);   if (readLine(tmp, sizeof tmp) && tmp[0]) strncpy(name, tmp, sizeof name - 1);
    printf("  Vehicle [%s] > ", veh);      if (readLine(tmp, sizeof tmp) && tmp[0]) strncpy(veh, tmp, sizeof veh - 1);
    printf("  Colour [%s] > ", col);       if (readLine(tmp, sizeof tmp) && tmp[0]) strncpy(col, tmp, sizeof col - 1);
    FILE* f = fopen(dst, "w");
    if (!f) { printf("Could not write %s\n", dst); return 1; }
    fprintf(f, "; mymp.ini — written by MyMP-Setup.exe\n\n[server]\nhost=%s\nport=%s\n\n[player]\nname=%s\nvehicle=%s\ncolor=%s\n",
            host, port, name, veh, col);
    fclose(f);
    printf("  + %s\n", dst);

launch:
    /* 4) launch GTA V */
    printf("\nLaunch GTA V now? [Y/n] > ");
    if (readLine(tmp, sizeof tmp) && (tmp[0] == 'n' || tmp[0] == 'N')) {
        printf("Done. Start GTA V whenever you like — MyMP will connect automatically.\n");
        return 0;
    }
    /* prefer Steam -applaunch (plays nicely with R* DRM), else run GTA5.exe directly */
    char steamExe[2048]; HKEY k; DWORD sz = sizeof steamExe; steamExe[0] = 0;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Valve\\Steam", 0, KEY_READ, &k) == ERROR_SUCCESS) {
        if (RegQueryValueExA(k, "SteamPath", NULL, NULL, (BYTE*)steamExe, &sz) == ERROR_SUCCESS) {
            strip(steamExe);
            pathJoin(tmp, sizeof tmp, steamExe, "steam.exe");
            if (!fileExists(tmp)) steamExe[0] = 0;
        } else steamExe[0] = 0;
        RegCloseKey(k);
    }
    STARTUPINFOA si = { sizeof si }; PROCESS_INFORMATION pi = { 0 };
    BOOL ok;
    if (steamExe[0]) {
        _snprintf(tmp, sizeof tmp, "\"%s\" -applaunch 271590", steamExe);
        ok = CreateProcessA(NULL, tmp, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
    } else {
        pathJoin(tmp, sizeof tmp, gta, "GTA5.exe");
        ok = CreateProcessA(NULL, tmp, NULL, NULL, FALSE, 0, NULL, gta, &si, &pi);
    }
    if (ok) {
        printf("GTA V launching. The MyMP client will load inside it.\n");
        printf("Connect in-game: press T, type /help\n");
        CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
    } else {
        printf("Could not launch GTA V (error %lu). Start it manually.\n", GetLastError());
    }
    printf("\nMyMP installed. Server console: run the MyMP server first (server artifacts).\n");
    return 0;
}
