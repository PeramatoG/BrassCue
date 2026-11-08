// BrassCue - GPL-3.0. Explicit UDP target with exception-safe source/interface binding.
#pragma once
#include "bridge_core.hpp"
#include "network_config.hpp"
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
    explicit UdpTarget(const std::string &ip, bool broadcast = false,
                       const std::string &source = {}, unsigned interface_index = 0) {
        validate_target(ip, broadcast);
        if (broadcast && source.empty())
            throw std::invalid_argument(
                "Broadcast requires an explicitly selected local interface");
#ifdef _WIN32
        WSADATA data{};
        if (WSAStartup(MAKEWORD(2, 2), &data) != 0)
            throw std::runtime_error("WSAStartup failed");
        wsa_started_ = true;
#endif
        try {
            address_.sin_family = AF_INET;
            address_.sin_port = htons(6454);
            if (inet_pton(AF_INET, ip.c_str(), &address_.sin_addr) != 1)
                throw std::invalid_argument("Invalid target IPv4");
            socket_ = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
#ifdef _WIN32
            if (socket_ == INVALID_SOCKET)
                throw std::runtime_error("Unable to create UDP socket");
#else
            if (socket_ < 0)
                throw std::runtime_error("Unable to create UDP socket");
#endif
            if (!source.empty()) {
                validate_target(source, false);
                sockaddr_in local{};
                local.sin_family = AF_INET;
                if (inet_pton(AF_INET, source.c_str(), &local.sin_addr) != 1 ||
                    ::bind(socket_, reinterpret_cast<sockaddr *>(&local), sizeof(local)) != 0)
                    throw std::runtime_error("Unable to bind selected IPv4 source");
            }
            if (interface_index) {
#ifdef IP_UNICAST_IF
                const auto index = htonl(interface_index);
                option(IPPROTO_IP, IP_UNICAST_IF, index,
                       "Unable to select outgoing network interface");
#else
                throw std::runtime_error("This platform cannot select an outgoing IPv4 interface");
#endif
            }
            if (broadcast) {
                const int enabled = 1;
                option(SOL_SOCKET, SO_BROADCAST, enabled, "Unable to enable UDP broadcast");
            }
        } catch (...) {
            close_socket();
            throw;
        }
    }
    explicit UdpTarget(const ResolvedNetwork &network)
        : UdpTarget(network.target, network.broadcast, network.source, network.interface_index) {}
    UdpTarget(const UdpTarget &) = delete;
    UdpTarget &operator=(const UdpTarget &) = delete;
    ~UdpTarget() {
        close_socket();
    }
    void send(const std::array<std::uint8_t, kPacketBytes> &packet) const {
#ifdef _WIN32
        auto count =
            ::sendto(socket_, reinterpret_cast<const char *>(packet.data()), int(packet.size()), 0,
                     reinterpret_cast<const sockaddr *>(&address_), sizeof(address_));
        if (count != int(packet.size()))
            throw std::runtime_error("Art-Net sendto failed");
#else
        auto count = ::sendto(socket_, packet.data(), packet.size(), 0,
                              reinterpret_cast<const sockaddr *>(&address_), sizeof(address_));
        if (count != static_cast<ssize_t>(packet.size()))
            throw std::runtime_error("Art-Net sendto failed");
#endif
    }

  private:
    template <class T> void option(int level, int name, const T &value, const char *error) {
#ifdef _WIN32
        if (setsockopt(socket_, level, name, reinterpret_cast<const char *>(&value),
                       sizeof(value)) != 0)
            throw std::runtime_error(error);
#else
        if (setsockopt(socket_, level, name, &value, sizeof(value)) != 0)
            throw std::runtime_error(error);
#endif
    }
    void close_socket() {
#ifdef _WIN32
        if (socket_ != INVALID_SOCKET) {
            closesocket(socket_);
            socket_ = INVALID_SOCKET;
        }
        if (wsa_started_) {
            WSACleanup();
            wsa_started_ = false;
        }
#else
        if (socket_ >= 0) {
            close(socket_);
            socket_ = -1;
        }
#endif
    }
#ifdef _WIN32
    SOCKET socket_ = INVALID_SOCKET;
    bool wsa_started_ = false;
#else
    int socket_ = -1;
#endif
    sockaddr_in address_{};
};
} // namespace brasscue
