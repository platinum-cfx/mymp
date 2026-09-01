// net.cpp — MyMP GTA V client networking implementation (original code)
#include "net.h"
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <cstdlib>
#include <cstring>

#pragma comment(lib, "ws2_32.lib")

namespace mymp {

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
