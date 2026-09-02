#!/usr/bin/env python3
"""
tools/gen_native_bindings.py — generate client/src/lua_native_bindings.cpp
from a checkout of the citizenfx/natives repository.

For every native with a parseable C signature this emits a typed Lua binding
(like FiveM's generated native wrappers): exact-signature invoke through the
runtime-discovered native table, with Lua <-> C argument marshaling per the
documented types. Two sorted lookup tables are emitted (by name and by hash),
so client Lua scripts can call any native:  mymp.native("SET_ENTITY_COORDS", ...)

Usage: python3 tools/gen_native_bindings.py /path/to/citizenfx-natives-checkout
"""
import os
import re
import sys

SIG_RE = re.compile(
    r"^\s*(?P<ret>[\w:*<>]+?)\s+(?P<name>_?0x[\w]+|_?[A-Z][A-Z0-9_]*)\s*\((?P<params>.*)\)\s*;?\s*$")

INTISH = {
    "int", "Any", "Hash", "Entity", "Ped", "Vehicle", "Object", "Blip", "Cam",
    "Player", "FireId", "Interior", "ScrHandle", "Pickup", "BOOL", "uint",
    "uchar", "ushort", "long", "TaskSequence", "PopGroup", "Prompt", "Model",
    "WeaponType", "eWeaponType", "eStats", "eGender", "eFiringPattern",
    "eDrivingMode", "eVehicleLockState", "eClothType", "eWeatherType", "ulong",
    "uchar", "uint8_t", "uint16_t", "uint32_t", "int8_t", "int16_t", "int32_t",
    "size_t", "Hash_t", "Camera", "VehicleNode", "Timer", "Group",
}

INTISH_PREFIX = ("E", "e", "Event")  # enums like ePedConfigFlag -> int-ish


def classify(t):
    """Return one of: 'int', 'float', 'string', 'vecout', 'unsupported'."""
    t = t.strip()
    if t.endswith("*"):
        base = t[:-1].strip()
        if base == "const char" or base == "char":
            return "string"
        if base in ("Vector3", "const Vector3"):
            return "vecoutptr"          # Vector3* (out) param
        if base == "float":
            return "floatptr"           # float* out-param
        if base == "BOOL":
            return "boolptr"            # BOOL* out-param
        if base == "Any":
            return "anyptr"             # Any* opaque pointer value
        return "intptr"                 # int*/Hash*/Entity*/Ped*/... out-params
    if t == "float":
        return "float"
    if t == "Vector3":
        return "vec3"                   # Vector3 return (hidden out-ptr) or value param
    if t in INTISH:
        return "int"
    if t.startswith(INTISH_PREFIX) or t == "char":
        return "int"
    return "int"  # unknown types are almost always enum/int — keep coverage


def split_params(params):
    """Split on commas at depth 0, handling 'const char*', 'int*' etc."""
    if not params.strip():
        return []
    out, depth, cur = [], 0, ""
    for ch in params:
        if ch == "(" or ch == "<":
            depth += 1
        elif ch == ")" or ch == ">":
            depth -= 1
        if ch == "," and depth == 0:
            out.append(cur.strip())
            cur = ""
        else:
            cur += ch
    if cur.strip():
        out.append(cur.strip())
    return out


def parse_param(p):
    """(type, name) or (type, None)."""
    p = p.strip()
    if p == "..." or p.startswith("..."):
        return ("varargs", None)
    # drop default values like = 0 (not present in db, but be safe)
    p = re.sub(r"\s*=.*$", "", p).strip()
    m = re.match(r"^(.*?)([A-Za-z_]\w*)\s*$", p)
    if not m:
        return (p.strip(), None)
    return (m.group(1).strip(), m.group(2))


def main():
    root = sys.argv[1] if len(sys.argv) > 1 else "/tmp/natives-repo"
    natives = []       # (name, docName, hash, ret, params)
    skipped = []
    by_name = {}
    by_doc = {}
    for dirpath, _dirs, files in os.walk(root):
        for fn in files:
            if not fn.endswith(".md"):
                continue
            path = os.path.join(dirpath, fn)
            try:
                text = open(path, encoding="utf-8", errors="replace").read()
            except OSError:
                continue
            name = None
            docName = fn[:-3] if fn.endswith(".md") else fn  # FiveM TitleCase name
            for line in text.splitlines():
                if line.startswith("## "):
                    parts = line[3:].strip().split()
                    if not parts:
                        break
                    cand = parts[0].upper()
                    if cand.startswith("_0X") or cand.startswith("0X"):
                        cand = parts[0]
                    name = cand
                    break
            if not name:
                continue
            m = HASH_RE.search(text)
            if not m:
                skipped.append(name)
                continue
            h = int(m.group(1), 16)
            sig = None
            for line in text.splitlines():
                if SIG_RE.match(line.strip()):
                    sig = line.strip()
                    break
            if not sig:
                skipped.append(name)
                continue
            sm = SIG_RE.match(sig)
            ret = sm.group("ret").strip()
            params = split_params(sm.group("params"))
            if name in by_name:
                continue
            if docName in by_doc:   # same TitleCase name for two natives
                continue
            by_name[name] = h
            by_doc[docName] = h
            natives.append((name, docName, h, ret, params))

    natives.sort(key=lambda kv: kv[0].lower())
    lines = []
    W = lines.append
    W("// lua_native_bindings.cpp — GENERATED by tools/gen_native_bindings.py from")
    W("// the citizenfx/natives repository (https://github.com/citizenfx/natives).")
    W("// Typed Lua bindings for every documented GTA V script native — the same")
    W("// approach FiveM uses (generated native wrappers). Hashes/types are factual")
    W("// game data; the call path is our own runtime-discovered native table.")
    W("#include \"lua/lua.h\"")
    W("#include \"lua/lauxlib.h\"")
    W("#include \"natives.h\"")
    W("#include \"natives_full.h\"")
    W("#include \"lua_native_bindings.h\"")
    W("")
    W("// matches the game's Vector3 layout (3 floats)")
    W("struct LuaVector3 { float x, y, z; };")
    W("")

    bindings = []  # (name, docName, hash)
    for name, docName, h, ret, params in natives:
        cls_ret = classify(ret)
        if cls_ret == "unsupported":
            cls_ret = "int" if ret in ("Any",) else "void"
        # v1: unsupported pointer returns -> treat as void (returns nil)
        if cls_ret in ("unsupported", "vecout"):
            pass
        plist = []  # (cname, cls, luaidx)
        has_varargs = False
        for i, p in enumerate(params):
            t, pn = parse_param(p)
            if t == "varargs":
                has_varargs = True
                continue
            cls = classify(t)
            plist.append((pn or f"a{i + 1}", cls, i + 1, t))
        fixed = plist

        bindings.append((name, docName, h))
        fn_name = "luaN_" + re.sub(r"[^A-Za-z0-9_]", "_", name)
        W(f"// {name}  0x{h:016X}")
        W(f"static int {fn_name}(lua_State* L) {{")
        required = [c for c in fixed if c[1] not in ("vecoutptr", "floatptr", "boolptr", "intptr", "anyptr")]
        if required:
            W(f"    const int n = lua_gettop(L);")
            reqw = "arg" if len(required) == 1 else "args"
            W(f"    if (n < {len(required)}) return luaL_error(L, \"{name}: expected at least {len(required)} {reqw}, got %d\", n);")
        cargs = []
        retlines = []  # code pushed AFTER the call: pointer out-params
        for cname, cls, idx, t in fixed:
            if cls == "float":
                W(f"    float {cname} = (float)luaL_checknumber(L, {idx});")
                cargs.append(cname)
            elif cls == "string":
                W(f"    const char* {cname} = luaL_checkstring(L, {idx});")
                cargs.append(cname)
            elif cls == "int" and t == "BOOL":
                # FiveM parity: BOOL params accept booleans, numbers, or nil
                W(f"    {classify_ctype(cls)} {cname};")
                W(f"    if (lua_isboolean(L, {idx})) {cname} = lua_toboolean(L, {idx}) ? 1 : 0;")
                W(f"    else if (lua_isnumber(L, {idx})) {cname} = ({classify_ctype(cls)})lua_tointeger(L, {idx});")
                W(f"    else {cname} = 0;")
                cargs.append(cname)
            elif cls == "int":
                W(f"    {classify_ctype(cls)} {cname} = ({classify_ctype(cls)})luaL_checkinteger(L, {idx});")
                cargs.append(cname)
            elif cls == "vec3":   # Vector3 value param -> vec3 userdata -> 3 float regs
                W(f"    LuaVector3 {cname};")
                W(f"    if (luaNativeToVec3(L, {idx}, &{cname}.x, &{cname}.y, &{cname}.z))")
                W(f"        return luaL_error(L, \"{name}: arg {idx} expected vec3\");")
                cargs.append(f"{cname}.x")
                cargs.append(f"{cname}.y")
                cargs.append(f"{cname}.z")
            elif cls == "vecoutptr":
                # Vector3* out-param: lenient input, returned as extra vec3
                W(f"    LuaVector3 {cname} = {{ 0, 0, 0 }};")
                W(f"    luaNativeToVec3(L, {idx}, &{cname}.x, &{cname}.y, &{cname}.z);")
                cargs.append(f"&{cname}")
                retlines.append(f"luaNativePushVec3(L, {cname}.x, {cname}.y, {cname}.z);")
            elif cls == "floatptr":
                # float* out-param: number in (else 0), returned as extra number
                W(f"    float {cname} = lua_isnumber(L, {idx}) ? (float)lua_tonumber(L, {idx}) : 0.0f;")
                cargs.append(f"&{cname}")
                retlines.append(f"lua_pushnumber(L, (lua_Number){cname});")
            elif cls == "boolptr":
                # BOOL* out-param: returned as extra boolean
                W(f"    int32_t {cname} = 0;")
                cargs.append(f"&{cname}")
                retlines.append(f"lua_pushboolean(L, {cname} != 0);")
            elif cls == "intptr":
                # int*/Hash*/Entity*/... out-params: returned as extra integer
                W(f"    int32_t {cname} = lua_isnumber(L, {idx}) ? (int32_t)lua_tointeger(L, {idx}) : 0;")
                cargs.append(f"&{cname}")
                retlines.append(f"lua_pushinteger(L, (lua_Integer){cname});")
            elif cls == "anyptr":
                # Any*: opaque pointer value — pass through, no read-back
                W(f"    uint64_t {cname} = lua_isnumber(L, {idx}) ? (uint64_t)lua_tointeger(L, {idx}) : 0;")
                cargs.append(f"(void*)(uintptr_t){cname}")
            else:  # unsupported param
                W(f"    return luaL_error(L, \"{name}: unsupported parameter type at arg {idx}\");")
        argc = ", ".join(cargs) if cargs else ""
        call = f"invoke<{ret_ctype(cls_ret)}>(0x{h:016X}ULL, {argc})" if argc else f"invoke<{ret_ctype(cls_ret)}>(0x{h:016X}ULL)"
        retn = 0
        if cls_ret in ("void", "unsupported"):
            W(f"    {call};")
        elif cls_ret == "float":
            W(f"    lua_pushnumber(L, (lua_Number)({call}));")
            retn = 1
        elif cls_ret == "string":  # const char* return -> Lua string
            W(f"    const char* s = {call};")
            W('    lua_pushstring(L, s ? s : "");')
            retn = 1
        elif cls_ret == "anyptr":  # Any* return -> opaque integer
            W(f"    lua_pushnumber(L, (lua_Number)(uintptr_t){call});")
            retn = 1
        elif cls_ret == "vec3":   # Vector3 return -> hidden out-ptr -> vec3 userdata
            W("    LuaVector3 r = { 0, 0, 0 };")
            call2 = call[:-1] + (", &r)" if argc else ", &r)")
            W(f"    {call2};")
            W("    luaNativePushVec3(L, r.x, r.y, r.z);")
            retn = 1
        elif ret == "BOOL":
            # FiveM parity: BOOL-returning natives give Lua booleans
            W(f"    lua_pushboolean(L, {call} != 0);")
            retn = 1
        else:
            W(f"    lua_pushnumber(L, (lua_Number)({call}));")
            retn = 1
        # pointer out-params come back as extra return values (FiveM parity)
        for rl in retlines:
            W(f"    {rl}")
            retn += 1
        W(f"    return {retn};")
        W("}")
        W("")

    # ---- by-name table (sorted case-insensitively) + by-hash table ----
    # (LuaNativeFn / LuaNativeEntry / LUA_NATIVES are declared in the
    #  hand-written header; the array itself is non-static so the runtime
    #  can register every native as a Lua global.)
    W("const LuaNativeEntry LUA_NATIVES[] = {")
    for name, docName, h in bindings:
        fn_name = "luaN_" + re.sub(r"[^A-Za-z0-9_]", "_", name)
        W(f'    {{"{name}", "{docName}", 0x{h:016X}ULL, {fn_name}}},')
    W("};")
    W("")
    W("// by-hash table, sorted by hash for binary search")
    W("static const LuaNativeEntry LUA_NATIVES_BY_HASH[] = {")
    for name, docName, h in sorted(bindings, key=lambda kv: kv[2]):
        fn_name = "luaN_" + re.sub(r"[^A-Za-z0-9_]", "_", name)
        W(f'    {{"{name}", "{docName}", 0x{h:016X}ULL, {fn_name}}},')
    W("};")
    W("")
    W("#ifdef _WIN32")
    W('#include <cstring>')
    W("#define NAT_STRCMP _stricmp")
    W("#else")
    W('#include <strings.h>')
    W("#define NAT_STRCMP strcasecmp")
    W("#endif")
    W("")
    W("// binary search by name (case-insensitive)")
    W("LuaNativeFn luaNativeByName(const char* name, uint64_t* hashOut) {")
    W("    size_t lo = 0, hi = LUA_NATIVES_COUNT;")
    W("    while (lo < hi) {")
    W("        size_t mid = (lo + hi) / 2;")
    W("        int c = NAT_STRCMP(name, LUA_NATIVES[mid].name);")
    W("        if (c == 0) { if (hashOut) *hashOut = LUA_NATIVES[mid].hash; return LUA_NATIVES[mid].fn; }")
    W("        if (c < 0) hi = mid; else lo = mid + 1;")
    W("    }")
    W("    return nullptr;")
    W("}")
    W("")
    W("// binary search by 64-bit hash")
    W("LuaNativeFn luaNativeByHash(uint64_t hash) {")
    W("    size_t lo = 0, hi = LUA_NATIVES_COUNT;")
    W("    while (lo < hi) {")
    W("        size_t mid = (lo + hi) / 2;")
    W("        uint64_t mh = LUA_NATIVES_BY_HASH[mid].hash;")
    W("        if (mh == hash) return LUA_NATIVES_BY_HASH[mid].fn;")
    W("        if (hash < mh) hi = mid; else lo = mid + 1;")
    W("    }")
    W("    return nullptr;")
    W("}")
    W("")

    dest = os.path.normpath(os.path.join(
        os.path.dirname(os.path.abspath(__file__)), "..", "client", "src",
        "lua_native_bindings.cpp"))
    with open(dest, "w") as f:
        f.write("\n".join(lines))
    print(f"wrote {dest}")
    print(f"bindings: {len(bindings)}  skipped (no hash/sig): {len(skipped)}")
    if skipped:
        print("skipped sample:", skipped[:8])


def ret_ctype(cls):
    return {"void": "void", "float": "float", "vec3": "void",
            "unsupported": "void", "string": "const char*", "int": "uint64_t",
            "anyptr": "uint64_t"}[cls]


def classify_ctype(cls):
    return "uint64_t"


# hash regex duplicated from gen_natives_full.py to keep this tool standalone
HASH_RE = re.compile(r"//\s*(0x[0-9A-Fa-f]{8,16})")


if __name__ == "__main__":
    main()
