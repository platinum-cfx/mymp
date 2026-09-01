// http_get.h — minimal HTTP GET for resource/asset downloads (FiveM-style
// resource streaming). Works on Windows (Winsock) and POSIX (tests).
#pragma once
#include <cstdint>
#include <string>

namespace mymp {

// GET http://host:port/path and return the body ("" on failure).
// Response must carry Content-Length; body is read to exactly that length.
std::string httpGet(const std::string& host, uint16_t port,
                    const std::string& path);

}  // namespace mymp
