// json.cpp — MyMP minimal JSON parser/serializer (cross-platform, original code)
#include "net.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace mymp {

// ================= JSON parsing (recursive descent) =================
namespace {
struct Parser {
    const char* p;
    const char* end;

    void skipWs() { while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) ++p; }

    bool parseValue(Json& v) {
        skipWs();
        if (p >= end) return false;
        char c = *p;
        if (c == '{') return parseObj(v);
        if (c == '[') return parseArr(v);
        if (c == '"') return parseStr(v);
        if (c == 't' || c == 'f') return parseBool(v);
        if (c == 'n') { v.type = Json::NUL; p += 4; return true; }
        return parseNum(v);
    }
    bool parseObj(Json& v) {
        v.type = Json::OBJ;
        ++p;  // {
        skipWs();
        if (p < end && *p == '}') { ++p; return true; }
        while (p < end) {
            Json key;
            if (!parseStr(key)) return false;
            skipWs();
            if (p >= end || *p != ':') return false;
            ++p;
            Json val;
            if (!parseValue(val)) return false;
            v.obj[key.str] = val;
            skipWs();
            if (p >= end) return false;
            if (*p == ',') { ++p; continue; }
            if (*p == '}') { ++p; return true; }
            return false;
        }
        return false;
    }
    bool parseArr(Json& v) {
        v.type = Json::ARR;
        ++p;  // [
        skipWs();
        if (p < end && *p == ']') { ++p; return true; }
        while (p < end) {
            Json val;
            if (!parseValue(val)) return false;
            v.arr.push_back(val);
            skipWs();
            if (p >= end) return false;
            if (*p == ',') { ++p; continue; }
            if (*p == ']') { ++p; return true; }
            return false;
        }
        return false;
    }
    bool parseStr(Json& v) {
        v.type = Json::STR;
        ++p;  // "
        std::string out;
        while (p < end && *p != '"') {
            if (*p == '\\' && p + 1 < end) {
                ++p;
                switch (*p) {
                    case 'n': out += '\n'; break;
                    case 't': out += '\t'; break;
                    case 'r': out += '\r'; break;
                    case '\\': out += '\\'; break;
                    case '"': out += '"'; break;
                    case '/': out += '/'; break;
                    case 'u': {
                        // minimal \uXXXX (ASCII subset)
                        if (p + 4 < end) {
                            char buf[5] = {p[1], p[2], p[3], p[4], 0};
                            out += (char)strtol(buf, nullptr, 16);
                            p += 4;
                        }
                        break;
                    }
                    default: out += *p;
                }
                ++p;
            } else {
                out += *p++;
            }
        }
        if (p >= end) return false;
        ++p;  // "
        v.str = out;
        return true;
    }
    bool parseBool(Json& v) {
        if (end - p >= 4 && strncmp(p, "true", 4) == 0) { v.type = Json::BOOL; v.b = true; p += 4; return true; }
        if (end - p >= 5 && strncmp(p, "false", 5) == 0) { v.type = Json::BOOL; v.b = false; p += 5; return true; }
        return false;
    }
    bool parseNum(Json& v) {
        char* q = nullptr;
        v.type = Json::NUM;
        v.num = strtod(p, &q);
        if (q == p) return false;
        p = q;
        return true;
    }
};
}  // namespace

bool Json::parse(const std::string& text) {
    Parser parser{text.c_str(), text.c_str() + text.size()};
    return parser.parseValue(*this);
}

// ---------- JSON serialization ----------
static void jsonWrite(std::string& out, const Json& v) {
    switch (v.type) {
        case Json::NUL: out += "null"; break;
        case Json::BOOL: out += v.b ? "true" : "false"; break;
        case Json::NUM: {
            char buf[48];
            if (v.num == (double)(long long)v.num)
                snprintf(buf, sizeof buf, "%lld", (long long)v.num);
            else
                snprintf(buf, sizeof buf, "%.6g", v.num);
            out += buf;
            break;
        }
        case Json::STR: out += '\"'; out += jsonEscape(v.str); out += '\"'; break;
        case Json::ARR: {
            out += '[';
            for (size_t i = 0; i < v.arr.size(); ++i) {
                if (i) out += ',';
                jsonWrite(out, v.arr[i]);
            }
            out += ']';
            break;
        }
        case Json::OBJ: {
            out += '{';
            bool first = true;
            for (const auto& kv : v.obj) {
                if (!first) out += ',';
                first = false;
                out += '\"'; out += jsonEscape(kv.first); out += "\":";
                jsonWrite(out, kv.second);
            }
            out += '}';
            break;
        }
    }
}

std::string Json::toJson() const {
    std::string out;
    jsonWrite(out, *this);
    return out;
}

std::string jsonEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if ((unsigned char)c < 0x20) {
                    char buf[8];
                    snprintf(buf, sizeof buf, "\\u%04x", (unsigned char)c);
                    out += buf;
                } else out += c;
        }
    }
    return out;
}


}  // namespace mymp
