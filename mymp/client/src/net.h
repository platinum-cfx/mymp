// net.h — MyMP GTA V client networking (original code, Windows Winsock)
// A tiny JSON value type + a UDP socket. No third-party dependencies.
#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace mymp {

// ---------- minimal JSON ----------
struct Json {
    enum Type { NUL, BOOL, NUM, STR, ARR, OBJ };
    Type type = NUL;
    bool b = false;
    double num = 0.0;
    std::string str;
    std::vector<Json> arr;
    std::map<std::string, Json> obj;

    const Json* get(const std::string& key) const {
        auto it = obj.find(key);
        return it == obj.end() ? nullptr : &it->second;
    }
    bool has(const std::string& key) const { return get(key) != nullptr; }
    double asNum(double def = 0.0) const {
        return type == NUM ? num : (type == STR ? atof(str.c_str()) : def);
    }
    std::string asStr(const std::string& def = "") const {
        return type == STR ? str : def;
    }
    // parse a JSON document into this value; returns false on error
    bool parse(const std::string& text);
    // serialize this value back to JSON text
    std::string toJson() const;
};

// escape a string for JSON output
std::string jsonEscape(const std::string& s);

// ---------- UDP socket ----------
class UdpSocket {
public:
    bool open(const std::string& host, uint16_t port);   // connected UDP
    bool send(const std::string& data);
    bool recv(std::string& out);                          // non-blocking
    void close();
    bool valid() const { return sock != 0; }

private:
    uintptr_t sock = 0;  // SOCKET (kept opaque to avoid winsock2 in headers)
};

}  // namespace mymp
