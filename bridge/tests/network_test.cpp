// BrassCue - GPL-3.0. Mask-aware routing policy and an isolated source-bound UDP request.
#include "../src/artnet_udp.hpp"
#include "../src/network_interfaces.hpp"
#include "check.hpp"
int main() {
    using namespace brasscue;
    CHECK(directed_broadcast("192.168.1.10", "255.255.255.0") == "192.168.1.255");
    CHECK(directed_broadcast("10.17.8.9", "255.255.0.0") == "10.17.255.255");
    CHECK(directed_broadcast("10.17.8.9", "255.255.254.0") == "10.17.9.255");
    CHECK(directed_broadcast("10.17.8.9", "255.255.255.128") == "10.17.8.127");
    CHECK(directed_broadcast("10.17.8.129", "255.255.255.192") == "10.17.8.191");
    for (auto mask : {"", "0.0.0.0", "255.255.255.254", "255.255.255.255", "255.0.255.0"}) {
        bool failed = false;
        try {
            directed_broadcast("10.1.1.1", mask);
        } catch (const std::exception &) {
            failed = true;
        }
        CHECK(failed);
    }
    std::vector<NetworkInterface> interfaces{
        {"A", "Ethernet A", "10.0.0.9", "255.255.255.128", 7, true, false, true},
        {"B", "Ethernet B", "10.1.0.9", "255.255.0.0", 8, true, false, true},
        {"LO", "Loopback", "127.0.0.1", "255.0.0.0", 1, true, true, false}};
    auto a = resolve_network("127.0.0.1", true, "A", interfaces);
    CHECK(a.target == "10.0.0.127" && a.source == "10.0.0.9" && a.interface_index == 7 &&
          a.broadcast);
    auto b = resolve_network("10.2.3.4", false, "B", interfaces);
    CHECK(b.source == "10.1.0.9" && b.interface_index == 8);
    CHECK(resolve_network("127.0.0.1", false, "", interfaces).source.empty());
    for (auto id : {"", "LO", "missing"}) {
        bool failed = false;
        try {
            resolve_network("127.0.0.1", true, id, interfaces);
        } catch (const std::exception &) {
            failed = true;
        }
        CHECK(failed);
    }
    interfaces[1].netmask.clear();
    CHECK(resolve_network("10.2.3.4", false, "B", interfaces).source == "10.1.0.9");
    bool unknown_mask_rejected = false;
    try {
        resolve_network("10.2.3.4", true, "B", interfaces);
    } catch (const std::exception &) {
        unknown_mask_rejected = true;
    }
    CHECK(unknown_mask_rejected);
    interfaces[0].active = false;
    bool failed = false;
    try {
        resolve_network("127.0.0.1", false, "A", interfaces);
    } catch (const std::exception &) {
        failed = true;
    }
    CHECK(failed);
    for (auto ip : {"", "256.0.0.1", "1.2.3", "1.2.3.4tail", "01.2.3.4", "0.0.0.0", "224.0.0.1",
                    "255.255.255.255"}) {
        failed = false;
        try {
            validate_target(ip, false);
        } catch (const std::exception &) {
            failed = true;
        }
        CHECK(failed);
    }
    validate_target("10.0.0.255", false); // May be a host on a non-/24 network: never infer a mask.
    auto actual = network_interfaces();
    unsigned loop_index = 0;
    for (const auto &nic : actual)
        if (nic.loopback && nic.address == "127.0.0.1")
            loop_index = nic.index;
    UdpTarget target("127.0.0.1", false, "127.0.0.1", loop_index);
    auto receiver = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
#ifdef _WIN32
    CHECK(receiver != INVALID_SOCKET);
    DWORD timeout = 1000;
    setsockopt(receiver, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char *>(&timeout),
               sizeof(timeout));
#else
    CHECK(receiver >= 0);
    timeval timeout{1, 0};
    setsockopt(receiver, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
#endif
    sockaddr_in local{};
    local.sin_family = AF_INET;
    local.sin_port = htons(6454);
    inet_pton(AF_INET, "127.0.0.1", &local.sin_addr);
    CHECK(::bind(receiver, reinterpret_cast<sockaddr *>(&local), sizeof(local)) == 0);
    target.send(build_artdmx(State{}, 0));
    std::array<char, 2048> bytes{};
    sockaddr_in peer{};
#ifdef _WIN32
    int length = sizeof(peer);
    auto count = recvfrom(receiver, bytes.data(), int(bytes.size()), 0,
                          reinterpret_cast<sockaddr *>(&peer), &length);
    closesocket(receiver);
#else
    socklen_t length = sizeof(peer);
    auto count = recvfrom(receiver, bytes.data(), bytes.size(), 0,
                          reinterpret_cast<sockaddr *>(&peer), &length);
    close(receiver);
#endif
    CHECK(count == 530 && ntohl(peer.sin_addr.s_addr) == ipv4_number("127.0.0.1"));
}
