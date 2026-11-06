#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>

namespace brasscue {

struct State {
    std::array<std::uint8_t, 4> rgbw{{0, 0, 0, 0}};
    std::uint8_t master = 255;
};

constexpr std::size_t kDmxSlots = 512;
constexpr std::size_t kArtDmxHeader = 18;
constexpr std::size_t kPacketBytes = kArtDmxHeader + kDmxSlots;

inline std::uint8_t apply_master(std::uint8_t raw, std::uint8_t master) {
    return static_cast<std::uint8_t>((static_cast<unsigned>(raw) * master + 127u) / 255u);
}

// Build one Art-Net ArtDmx packet. The first four DMX slots carry R/G/B/W.
inline std::array<std::uint8_t, kPacketBytes> build_artdmx(const State& state,
                                                           std::uint16_t port_address) {
    if (port_address > 32767) {
        throw std::invalid_argument("Art-Net Port-Address must be 0..32767");
    }

    std::array<std::uint8_t, kPacketBytes> packet{};
    constexpr std::uint8_t id[] = {'A', 'r', 't', '-', 'N', 'e', 't', 0};
    for (std::size_t i = 0; i < sizeof(id); ++i) packet[i] = id[i];
    packet[8] = 0x00;  // ArtDmx opcode, little endian (0x5000)
    packet[9] = 0x50;
    packet[10] = 0x00; // Protocol version 14, big endian
    packet[11] = 0x0e;
    packet[12] = 0x00; // Sequence disabled for this simulator
    packet[13] = 0x00; // Physical input
    packet[14] = static_cast<std::uint8_t>(port_address & 0xff);
    packet[15] = static_cast<std::uint8_t>(port_address >> 8);
    packet[16] = 0x02; // DMX length = 512, big endian
    packet[17] = 0x00;

    for (std::size_t i = 0; i < state.rgbw.size(); ++i)
        packet[kArtDmxHeader + i] = apply_master(state.rgbw[i], state.master);

    return packet;
}

} // namespace brasscue
