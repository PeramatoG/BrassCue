#include "bridge_core.hpp"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <sstream>
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

namespace {

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
            throw std::invalid_argument("--target must be an IPv4 address");
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
        closesocket(socket_);
        WSACleanup();
#else
        close(socket_);
#endif
    }

    void send(const std::array<std::uint8_t, brasscue::kPacketBytes>& packet) const {
#ifdef _WIN32
        const auto count = ::sendto(socket_, reinterpret_cast<const char*>(packet.data()),
                                   static_cast<int>(packet.size()), 0,
                                   reinterpret_cast<const sockaddr*>(&address_), sizeof(address_));
        if (count == SOCKET_ERROR) throw std::runtime_error("Art-Net sendto failed");
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

unsigned parse_uint(const std::string& text, unsigned max) {
    std::size_t end = 0;
    const auto number = std::stoul(text, &end, 0);
    if (end != text.size() || number > max) throw std::invalid_argument("Value out of range: " + text);
    return static_cast<unsigned>(number);
}

void show(const brasscue::State& state) {
    std::cout << "Raw RGBW:";
    for (const auto value : state.rgbw) std::cout << " " << static_cast<int>(value);
    std::cout << " | Grandmaster: " << static_cast<int>(state.master) << " | Output RGBW:";
    for (const auto value : state.rgbw)
        std::cout << " " << static_cast<int>(brasscue::apply_master(value, state.master));
    std::cout << "\n";
}

void usage() {
    std::cout << "BrassCue Bridge - simulated controls (no Arduino required)\n"
              << "Usage: brasscue-bridge [--simulate] [--target IPv4] [--universe 0..32767]\n"
              << "No network traffic unless --target is specified. UDP Art-Net port 6454.\n"
              << "Commands: r/g/b/w/m <0..255> (0xFF also works), show, send, help, quit\n";
}

} // namespace

int main(int argc, char** argv) {
    try {
        std::string target;
        std::uint16_t universe = 0;
        for (int i = 1; i < argc; ++i) {
            const std::string arg = argv[i];
            if (arg == "--help" || arg == "-h") { usage(); return 0; }
            if (arg == "--simulate") continue;
            if (arg == "--target" && i + 1 < argc) { target = argv[++i]; continue; }
            if (arg == "--universe" && i + 1 < argc) {
                universe = static_cast<std::uint16_t>(parse_uint(argv[++i], 32767));
                continue;
            }
            throw std::invalid_argument("Unknown or incomplete argument: " + arg);
        }

        std::unique_ptr<UdpTarget> udp;
        if (!target.empty()) udp = std::make_unique<UdpTarget>(target);

        brasscue::State state;
        usage();
        show(state);

        auto emit = [&] {
            if (!udp) {
                std::cout << "[simulation only — network disabled]\n";
            } else {
                udp->send(brasscue::build_artdmx(state, universe));
                std::cout << "[sent ArtDmx to " << target << ":6454, port-address "
                          << universe << "]\n";
            }
        };

        std::string line;
        while (std::cout << "> " && std::getline(std::cin, line)) {
            std::istringstream input(line);
            std::string command, value, trailing;
            if (!(input >> command)) continue;
            if (command == "quit" || command == "exit") break;
            if (command == "help") { usage(); continue; }
            if (command == "show") { show(state); continue; }
            if (command == "send") { emit(); continue; }
            if (command.size() != 1 || std::string("rgbwm").find(command[0]) == std::string::npos ||
                !(input >> value) || (input >> trailing)) {
                std::cout << "Invalid command. Type help.\n";
                continue;
            }
            try {
                const auto v = static_cast<std::uint8_t>(parse_uint(value, 255));
                if (command == "m") state.master = v;
                else {
                    const auto channel = std::string("rgbw").find(command[0]);
                    state.rgbw[channel] = v;
                }
                show(state);
                emit();
            } catch (const std::exception& ex) {
                std::cout << "Invalid value: " << ex.what() << "\n";
            }
        }
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "BrassCue Bridge: " << ex.what() << "\n";
        return 1;
    }
}
