// http_get.cpp — minimal HTTP GET (Winsock on Windows, POSIX sockets on
// other platforms so the download path can be integration-tested on Linux).
#include "http_get.h"

#include <cstdio>
#include <cstring>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace mymp {

std::string httpGet(const std::string& host, uint16_t port,
                    const std::string& path) {
#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return "";
    SOCKET fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == INVALID_SOCKET) { WSACleanup(); return ""; }
#else
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return "";
#endif
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof addr);
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    struct hostent* he = gethostbyname(host.c_str());
    if (!he) {
#ifdef _WIN32
        closesocket(fd); WSACleanup();
#else
        close(fd);
#endif
        return "";
    }
    memcpy(&addr.sin_addr, he->h_addr, (size_t)he->h_length);

    int ok = 0;
#ifdef _WIN32
    ok = connect(fd, (struct sockaddr*)&addr, sizeof addr) == 0;
#else
    ok = connect(fd, (struct sockaddr*)&addr, sizeof addr) == 0;
#endif
    if (!ok) {
#ifdef _WIN32
        closesocket(fd); WSACleanup();
#else
        close(fd);
#endif
        return "";
    }

    std::string req = "GET " + path + " HTTP/1.1\r\n"
                      "Host: " + host + ":" + std::to_string(port) + "\r\n"
                      "Connection: close\r\n"
                      "User-Agent: MyMP/1.0\r\n"
                      "\r\n";
    if (send(fd, req.c_str(), (int)req.size(), 0) != (int)req.size()) {
#ifdef _WIN32
        closesocket(fd); WSACleanup();
#else
        close(fd);
#endif
        return "";
    }

    std::string raw;
    char buf[4096];
    for (;;) {
        int n = recv(fd, buf, sizeof buf, 0);
        if (n <= 0) break;
        raw.append(buf, (size_t)n);
        if (raw.size() > (1u << 20) * 4) break;  // 4 MB cap
    }
#ifdef _WIN32
    closesocket(fd); WSACleanup();
#else
    close(fd);
#endif

    // parse "HTTP/1.1 200 OK" + headers, find Content-Length
    size_t hdrEnd = raw.find("\r\n\r\n");
    if (hdrEnd == std::string::npos) return "";
    std::string headers = raw.substr(0, hdrEnd);
    if (headers.find(" 200 ") == std::string::npos &&
        headers.find(" 200\r\n") == std::string::npos &&
        headers.compare(0, 12, "HTTP/1.1 200") != 0)
        return "";
    size_t cl = headers.find("Content-Length:");
    if (cl == std::string::npos) return "";
    long len = atol(headers.c_str() + cl + 15);
    if (len < 0) return "";
    std::string body = raw.substr(hdrEnd + 4);
    if ((long)body.size() > len) body.resize((size_t)len);
    return body;
}

}  // namespace mymp
