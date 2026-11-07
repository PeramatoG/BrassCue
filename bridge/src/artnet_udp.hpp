#pragma once

// Shared UDP Art-Net sender for the CLI and wxWidgets desktop front end.
// No implicit broadcast or transmission: callers explicitly create a target.
#include "bridge_core.hpp"
#include <array>
#include <cstdint>
#include <stdexcept>
#include <string>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace brasscue {

class UdpTarget {
public:
    explicit UdpTarget(const std::string& ipv4) {
#ifdef _WIN32
        WSADATA data{};
        if (WSAStartup(MAKEWORD(2, 2), &data) != 0)
            throw std::runtime_error("WSAStartup failed");
#endif
        address_ = {};
        address_.sin_family = AF_INET;
        address_.sin_port = htons(6454);
        if (inet_pton(AF_INET, ipv4.c_str(), &address_.sin_addr) != 1) {
#ifdef _WIN32
            WSACleanup();
#endif
            throw std::invalid_argument("Art-Net target must be an IPv4 address");
        }
        socket_ = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
#ifdef _WIN32
        if (socket_ == INVALID_SOCKET) {
            WSACleanup();
            throw std::runtime_error("Unable to create UDP socket");
        }
#else
        if (socket_ < 0) throw std::runtime_error("Unable to create UDP socket");
#endif
    }

    UdpTarget(const UdpTarget&) = delete;
    UdpTarget& operator=(const UdpTarget&) = delete;

    ~UdpTarget() {
#ifdef _WIN32
        if (socket_ != INVALID_SOCKET) closesocket(socket_);
        WSACleanup();
#else
        if (socket_ >= 0) close(socket_);
#endif
    }

    void send(const std::array<std::uint8_t, kPacketBytes>& packet) const {
#ifdef _WIN32
        const auto count = ::sendto(socket_, reinterpret_cast<const char*>(packet.data()),
                                   static_cast<int>(packet.size()), 0,
                                   reinterpret_cast<const sockaddr*>(&address_), sizeof(address_));
        if (count != static_cast<int>(packet.size()))
            throw std::runtime_error("Art-Net sendto failed");
#else
        const auto count = ::sendto(socket_, packet.data(), packet.size(), 0,
                                   reinterpret_cast<const sockaddr*>(&address_), sizeof(address_));
        if (count != static_cast<ssize_t>(packet.size()))
            throw std::runtime_error("Art-Net sendto failed");
#endif
    }

private:
#ifdef _WIN32
    SOCKET socket_ = INVALID_SOCKET;
#else
    int socket_ = -1;
#endif
    sockaddr_in address_{};
};

} // namespace brasscue
