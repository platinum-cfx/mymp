// scriptthread.cpp — MyMP GTA V client: script-thread hook (original code).
//
// Approach (the same family of technique ScriptHookV-style mods use):
//   1. scan GTA5.exe code for the script-thread pool access sequence,
//   2. take a live thread from the pool,
//   3. swap its vtable for one whose Run entry calls our callback first,
//      then the original Run.
//
// Pattern / vtable index below are the values used by the community for the
// b2944-era builds; if a future game update changes them, update the two
// constants at the top — the code degrades gracefully (falls back to a
// worker thread) if anything fails.

#include "scriptthread.h"
#define WIN32_LEAN_AND_MEAN
#include <cstdint>
#include <windows.h>
#include <cstring>

namespace mymp {

static void (*g_cb)() = nullptr;
static uintptr_t* g_origVtable = nullptr;
static uintptr_t* g_newVtable = nullptr;

// The script-thread pool access sequence seen in GTA5.exe main loop.
// 48 8B 05 disp32      mov rax,[rip+disp32]     ; rax = pool ptr
// 48 8B 0C C8          mov rcx,[rax+rcx*8]      ; rcx = thread[index]
// 48 8D 04 D1          lea rax,[rcx+rdx*8]
// 48 85 C0             test rax,rax
// 74 08                jz +8
// Bytes 3..6 (disp32) are wildcards. VERIFY on your game build.
static const unsigned char kPoolPattern[] = {
    0x48, 0x8B, 0x05, 0x00, 0x00, 0x00, 0x00,
    0x48, 0x8B, 0x0C, 0xC8,
    0x48, 0x8D, 0x04, 0xD1,
    0x48, 0x85, 0xC0,
    0x74, 0x08,
};
static const size_t kPoolDispOff = 3;
static const int kPoolEntries = 64;      // scan this many pool slots
static const int kMaxVtableSlots = 32;   // sanity cap for the vtable copy
static const size_t kVtableRunIndex = 4; // GTAThread::Run vtable slot (verify!)

static uint8_t* findPattern(uint8_t* start, size_t size,
                            const unsigned char* pat, size_t patLen) {
    for (size_t off = 0; off + patLen <= size; ++off) {
        size_t m = 0;
        while (m < patLen) {
            if (m >= kPoolDispOff && m < kPoolDispOff + 4) { ++m; continue; }  // wildcard
            if (start[off + m] != pat[m]) break;
            ++m;
        }
        if (m == patLen) return start + off;
    }
    return nullptr;
}

// Our Run replacement: call the MyMP tick, then the original Run.
static void __fastcall runTrampoline(void* self) {
    if (g_cb) g_cb();
    auto orig = (void(__fastcall*)(void*))g_origVtable[kVtableRunIndex];
    if (orig) orig(self);
}

bool installScriptTick(void (*callback)()) {
    if (!callback) return false;
    uint8_t* base = (uint8_t*)GetModuleHandleA("GTA5.exe");
    if (!base) return false;
    IMAGE_DOS_HEADER* dos = (IMAGE_DOS_HEADER*)base;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return false;
    IMAGE_NT_HEADERS* nt = (IMAGE_NT_HEADERS*)(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return false;

    uint8_t* codeStart = base + nt->OptionalHeader.BaseOfCode;
    size_t codeSize = nt->OptionalHeader.SizeOfCode;
    uint8_t* hit = findPattern(codeStart, codeSize, kPoolPattern, sizeof kPoolPattern);
    if (!hit) return false;

    // read the pool pointer from the rip-relative displacement
    int32_t disp;
    memcpy(&disp, hit + kPoolDispOff, 4);
    uint8_t* poolPtr = hit + kPoolDispOff + 4 + disp;
    uintptr_t pool;
    memcpy(&pool, poolPtr, sizeof pool);
    if (!pool) return false;

    // find a live thread and keep its vtable
    uintptr_t thread = 0;
    for (int i = 0; i < kPoolEntries; ++i) {
        uintptr_t slot;
        memcpy(&slot, (uint8_t*)pool + i * sizeof(uintptr_t), sizeof slot);
        if (slot) { thread = slot; break; }
    }
    if (!thread) return false;
    uintptr_t vtable;
    memcpy(&vtable, (void*)thread, sizeof vtable);
    if (!vtable) return false;

    // copy the vtable, replace Run with our trampoline
    uintptr_t* copy = new uintptr_t[kMaxVtableSlots];
    for (int i = 0; i < kMaxVtableSlots; ++i) copy[i] = ((uintptr_t*)vtable)[i];
    if (copy[kVtableRunIndex] == (uintptr_t)&runTrampoline) { delete[] copy; return true; }

    g_origVtable = (uintptr_t*)vtable;
    g_newVtable = copy;
    g_cb = callback;
    g_newVtable[kVtableRunIndex] = (uintptr_t)&runTrampoline;

    // swap the thread's vtable pointer (one thread is enough — Run is shared)
    uintptr_t* threadVtable = (uintptr_t*)thread;
    DWORD oldProtect;
    if (!VirtualProtect(threadVtable, sizeof(uintptr_t), PAGE_READWRITE, &oldProtect)) {
        delete[] copy;
        g_origVtable = g_newVtable = nullptr;
        g_cb = nullptr;
        return false;
    }
    threadVtable[0] = (uintptr_t)g_newVtable;
    VirtualProtect(threadVtable, sizeof(uintptr_t), oldProtect, &oldProtect);
    return true;
}

}  // namespace mymp
