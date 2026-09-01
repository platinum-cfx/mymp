#!/usr/bin/env python3
"""
Generate client/src/natives.h from the citizenfx/natives documentation repo.

This is the ONLY piece of citizenfx data used in the MyMP client — the native
hashes are factual game data published in that repo (the same data all GTA mod
tools use). Everything else in the client is original code.

The hash values below were verified against the citizenfx/natives repository on
2026-08-31. The generator refreshes them from raw.githubusercontent.com (not
rate-limited) when possible, and falls back to the verified values.

Usage:  python3 tools/gen_natives.py   (writes client/src/natives.h)
"""
import os
import re
import sys
import urllib.request

RAW = "https://raw.githubusercontent.com/citizenfx/natives/master"

# native name -> (namespace folder, verified hash, repo filename)
# Verified by fetching from citizenfx/natives on 2026-08-31.
VERIFIED = {
    "PLAYER_PED_ID": ("PLAYER", 0xD80958FC74E988A6, "PlayerPedId.md"),
    "PLAYER_ID": ("PLAYER", 0x4F8644AF03D0E0D6, "PlayerId.md"),
    "SET_PLAYER_CONTROL": ("PLAYER", 0x8D32347D6D4C40A2, "SetPlayerControl.md"),
    "GET_PLAYER_WANTED_LEVEL": ("PLAYER", 0xE28E54788CE8F12D, "GetPlayerWantedLevel.md"),
    "SET_PLAYER_WANTED_LEVEL": ("PLAYER", 0x39FF19C64EF7DA5B, "SetPlayerWantedLevel.md"),
    "DOES_ENTITY_EXIST": ("ENTITY", 0x7239B21A38F536BA, "DoesEntityExist.md"),
    "GET_ENTITY_COORDS": ("ENTITY", 0x3FEF770D40960D5A, "GetEntityCoords.md"),
    "SET_ENTITY_COORDS": ("ENTITY", 0x06843DA7060A026B, "SetEntityCoords.md"),
    "GET_ENTITY_HEADING": ("ENTITY", 0xE83D4F9BA2A38914, "GetEntityHeading.md"),
    "SET_ENTITY_HEADING": ("ENTITY", 0x8E2530AA8ADA980E, "SetEntityHeading.md"),
    "GET_ENTITY_SPEED": ("ENTITY", 0xD5037BA82E12416F, "GetEntitySpeed.md"),
    "GET_ENTITY_MODEL": ("ENTITY", 0x9F47B058362C84B5, "GetEntityModel.md"),
    "SET_ENTITY_VELOCITY": ("ENTITY", 0x1C99BB7B6E96D16F, "SetEntityVelocity.md"),
    "GET_ENTITY_VELOCITY": ("ENTITY", 0x4805D2B1D8CF94A9, "GetEntityVelocity.md"),
    "SET_ENTITY_AS_MISSION_ENTITY": ("ENTITY", 0xAD738C3085FE7E11, "SetEntityAsMissionEntity.md"),
    "SET_ENTITY_COLLISION": ("ENTITY", 0x1A9205C1B9EE827F, "SetEntityCollision.md"),
    "FREEZE_ENTITY_POSITION": ("ENTITY", 0x428CA6DBD1094446, "FreezeEntityPosition.md"),
    "DELETE_ENTITY": ("ENTITY", 0xAE3CBE5BF394C9C9, "DeleteEntity.md"),
    "SET_ENTITY_LOAD_COLLISION_FLAG": ("ENTITY", 0x0DC7CABAB1E9B67E, "SetEntityLoadCollisionFlag.md"),
    "SET_ENTITY_AS_NO_LONGER_NEEDED": ("ENTITY", 0xB736A491E64A32CF, "SetEntityAsNoLongerNeeded.md"),
    "CREATE_VEHICLE": ("VEHICLE", 0xAF35D0D2583051B0, "CreateVehicle.md"),
    "SET_VEHICLE_FORWARD_SPEED": ("VEHICLE", 0xAB54A438726D25D5, "SetVehicleForwardSpeed.md"),
    "SET_VEHICLE_ENGINE_ON": ("VEHICLE", 0x2497C4717C8B881E, "SetVehicleEngineOn.md"),
    "SET_VEHICLE_ON_GROUND_PROPERLY": ("VEHICLE", 0x49733E92263139D1, "SetVehicleOnGroundProperly.md"),
    "SET_VEHICLE_CUSTOM_PRIMARY_COLOUR": ("VEHICLE", 0x7141766F91D15BEA, "SetVehicleCustomPrimaryColour.md"),
    "SET_VEHICLE_CUSTOM_SECONDARY_COLOUR": ("VEHICLE", 0x36CED73BFED89754, "SetVehicleCustomSecondaryColour.md"),
    "SET_VEHICLE_IS_CONSIDERED_BY_PLAYER": ("VEHICLE", 0x31B927BBC44156CD, "SetVehicleIsConsideredByPlayer.md"),
    "CREATE_PED": ("PED", 0xD49F9B0955C367DE, "CreatePed.md"),
    "SET_PED_INTO_VEHICLE": ("PED", 0xF75B0D629E1C063D, "SetPedIntoVehicle.md"),
    "IS_PED_IN_ANY_VEHICLE": ("PED", 0x997ABD671D25CA0B, "IsPedInAnyVehicle.md"),
    "GET_VEHICLE_PED_IS_IN": ("PED", 0x9A9112A0FE9A4713, "GetVehiclePedIsIn.md"),
    "REQUEST_MODEL": ("STREAMING", 0x963D27A58DF860AC, "RequestModel.md"),
    "HAS_MODEL_LOADED": ("STREAMING", 0x98A4EB5D89A0C952, "HasModelLoaded.md"),
    "SET_MODEL_AS_NO_LONGER_NEEDED": ("STREAMING", 0xE532F5D78798DAAB, "SetModelAsNoLongerNeeded.md"),
    "IS_MODEL_A_VEHICLE": ("STREAMING", 0x19AAC8F07BFEC53E, "IsModelAVehicle.md"),
    "CLEAR_PED_TASKS_IMMEDIATELY": ("TASK", 0xAAA34F8A7CB32098, "ClearPedTasksImmediately.md"),
    "GET_ENTITY_HEALTH": ("ENTITY", 0xEEF059FAD016D209, "GetEntityHealth.md"),
    "SET_ENTITY_HEALTH": ("ENTITY", 0x6B76DC1F3AE6E6A3, "SetEntityHealth.md"),
    "GET_PED_ARMOUR": ("PED", 0x9483AF821605B1D8, "GetPedArmour.md"),
    "SET_PED_ARMOUR": ("PED", 0xCEA04D83135264CC, "SetPedArmour.md"),
    "GIVE_WEAPON_TO_PED": ("WEAPON", 0xBF0FD6E56C964FCB, "GiveWeaponToPed.md"),
    "BEGIN_TEXT_COMMAND_DISPLAY_HELP": ("HUD", 0x8509B634FBE7DA11, "BeginTextCommandDisplayHelp.md"),
    "ADD_TEXT_COMPONENT_SUBSTRING_PLAYER_NAME": ("HUD", 0x6C188BE134E074AA, "AddTextComponentSubstringPlayerName.md"),
    "END_TEXT_COMMAND_DISPLAY_HELP": ("HUD", 0x238FFE5C7B0498A6, "EndTextCommandDisplayHelp.md"),
    "SET_FAKE_WANTED_LEVEL": ("MISC", 0x1454F2448DE30163, "SetFakeWantedLevel.md"),
    # --- chat UI (2026-09-01, verified from local citizenfx/natives clone) ---
    "BEGIN_TEXT_COMMAND_DISPLAY_TEXT": ("HUD", 0x25FBB336DF1804CB, "BeginTextCommandDisplayText.md"),
    "END_TEXT_COMMAND_DISPLAY_TEXT": ("HUD", 0xCD015E5BB0D96A57, "EndTextCommandDisplayText.md"),
    "SET_TEXT_SCALE": ("HUD", 0x07C837F9A01C34C9, "SetTextScale.md"),
    "SET_TEXT_COLOUR": ("HUD", 0xBE6B23FFA53FB442, "SetTextColour.md"),
    "SET_TEXT_FONT": ("HUD", 0x66E0276CC5F6B9DA, "SetTextFont.md"),
    "SET_TEXT_OUTLINE": ("HUD", 0x2513DFB0FB8400FE, "SetTextOutline.md"),
    "SET_TEXT_CENTRE": ("HUD", 0xC02F4DBFB51D988B, "SetTextCentre.md"),
    "SET_TEXT_WRAP": ("HUD", 0x63145D9C883A1A70, "SetTextWrap.md"),
    "DRAW_RECT": ("GRAPHICS", 0x3A618A217E5154F0, "DrawRect.md"),
    "GET_SCREEN_RESOLUTION": ("GRAPHICS", 0x888D57E407E63624, "GetScreenResolution.md"),
    # --- combat (2026-09-01) ---
    "HAS_ENTITY_BEEN_DAMAGED_BY_ENTITY": ("ENTITY", 0xC86D67D52A707CF8, "HasEntityBeenDamagedByEntity.md"),
    "CLEAR_ENTITY_LAST_DAMAGE_ENTITY": ("ENTITY", 0xA72CD9CA74A5ECBA, "ClearEntityLastDamageEntity.md"),
    "RESURRECT_PED": ("PED", 0x71BC8E838B9C6035, "ResurrectPed.md"),
    "DISABLE_CONTROL_ACTION": ("PAD", 0xFE99B66D079CF6BC, "DisableControlAction.md"),
    "CREATE_OBJECT": ("OBJECT", 0x509D5878EB39E842, "CreateObject.md"),
    "ENABLE_CONTROL_ACTION": ("PAD", 0x351220255D64C155, "EnableControlAction.md"),
}

HASH_RE = re.compile(r"0x([0-9A-Fa-f]{16})")


def http_text(url):
    req = urllib.request.Request(url, headers={"User-Agent": "mymp-gen"})
    with urllib.request.urlopen(req, timeout=30) as r:
        return r.read().decode("utf-8", "replace")


def refresh(native):
    """Try to re-fetch the hash from the repo; return verified value on failure."""
    ns, fallback, fname = VERIFIED[native]
    try:
        md = http_text(f"{RAW}/{ns}/{fname}")
        m = HASH_RE.search(md)
        if m:
            h = int(m.group(1), 16)
            if h == fallback:
                print(f"  ok   {ns}/{fname} -> 0x{h:016X} (matches verified)")
            else:
                print(f"  diff {ns}/{fname} -> 0x{h:016X} (verified was 0x{fallback:016X}); using repo value")
            return h
        print(f"  warn {ns}/{fname}: no hash found, keeping verified 0x{fallback:016X}")
    except Exception as e:
        print(f"  keep {ns}/{fname}: {e}; keeping verified 0x{fallback:016X}")
    return fallback


def main():
    out_path = os.path.join(os.path.dirname(__file__), "..", "client", "src", "natives.h")
    lines = [
        "// natives.h — GENERATED by tools/gen_natives.py from",
        "// the citizenfx/natives documentation repository (https://github.com/citizenfx/natives).",
        "// Native hashes are factual game data; all other code in the client is original.",
        "#pragma once",
        "#include <cstdint>",
        "",
    ]
    for native, (ns, _, _) in VERIFIED.items():
        h = refresh(native)
        lines.append(f"constexpr uint64_t N_{native} = 0x{h:016X}ULL;  // {ns}")
    lines += [
        "",
        "// Runtime-discovered native call table (set by client.cpp).",
        "extern uint64_t (*g_nativeTable[256][256])();",
        "",
        "template <typename R, typename... A>",
        "inline R invoke(uint64_t hash, A... args) {",
        "    auto fn = reinterpret_cast<R (*)(A...)>(g_nativeTable[(hash >> 8) & 0xFF][hash & 0xFF]);",
        "    if (!fn) return R{};",
        "    return fn(args...);",
        "}",
        "",
    ]
    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    with open(out_path, "w") as f:
        f.write("\n".join(lines))
    print(f"\nWrote {out_path} with {len(VERIFIED)} natives")


if __name__ == "__main__":
    main()
