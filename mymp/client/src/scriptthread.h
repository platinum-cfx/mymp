// scriptthread.h — MyMP GTA V client: run code inside a GTA script thread.
//
// Community reversing (GTAForums/UC threads) shows many GTA V natives —
// e.g. REQUEST_MODEL — require the calling thread to be a running
// GTAThread with CGameScriptHandler in TLS; calling them from a raw
// worker thread crashes the game. This module hooks a script thread's
// Run (vtable swap) and invokes our callback from inside it.
#pragma once

namespace mymp {

// Installs a callback invoked from inside a running GTA script thread.
// Returns true if the hook was installed; false to fall back to a worker
// thread (current behaviour, minus the script context).
bool installScriptTick(void (*callback)());

}  // namespace mymp
