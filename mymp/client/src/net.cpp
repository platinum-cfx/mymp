// net.cpp — MyMP GTA V client networking implementation (original code)
#include "net.h"
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <cstdlib>
#include <cstring>

#pragma comment(lib, "ws2_32.lib")

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

// ================= UDP socket =================
bool UdpSocket::open(const std::string& host, uint16_t port) {
    WSADATA wsa{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return false;
    SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == INVALID_SOCKET) return false;
    u_long nonblock = 1;
    ioctlsocket(s, FIONBIO, &nonblock);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(host.c_str());
    if (addr.sin_addr.s_addr == INADDR_NONE) {
        // resolve hostname
        struct addrinfo hints{}, *res = nullptr;
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_DGRAM;
        if (getaddrinfo(host.c_str(), nullptr, &hints, &res) != 0 || !res) {
            closesocket(s);
            return false;
        }
        addr.sin_addr = ((sockaddr_in*)res->ai_addr)->sin_addr;
        freeaddrinfo(res);
    }
    // "connected" UDP: only accept datagrams from this server
    if (connect(s, (sockaddr*)&addr, sizeof addr) == SOCKET_ERROR) {
        closesocket(s);
        return false;
    }
    sock = (uintptr_t)s;
    return true;
}

bool UdpSocket::send(const std::string& data) {
    if (!sock) return false;
    int n = ::send((SOCKET)sock, data.c_str(), (int)data.size(), 0);
    return n == (int)data.size();
}

bool UdpSocket::recv(std::string& out) {
    if (!sock) return false;
    char buf[8192];
    int n = ::recv((SOCKET)sock, buf, sizeof buf - 1, 0);
    if (n <= 0) return false;
    buf[n] = 0;
    out.assign(buf, (size_t)n);
    return true;
}

void UdpSocket::close() {
    if (sock) {
        closesocket((SOCKET)sock);
        sock = 0;
    }
    WSACleanup();
}

}  // namespace mymp
