// BrassCue - GPL-3.0. Canonical RGBW group definitions and deterministic derived assignments.
#pragma once
#include "patch.hpp"
#include "utf8.hpp"
#include <algorithm>
#include <array>
#include <set>
namespace brasscue {
using EncoderPatch = std::array<std::vector<Assignment>, 4>;
enum class FixtureBoundary { Reject };
struct FixtureBlock {
    unsigned universe = 1, address = 5, count = 1;
};
struct FixtureGroup {
    std::string name = "HOUSE PANELS";
    std::string order = "RGBW";
    unsigned stride = 4;
    FixtureBoundary boundary = FixtureBoundary::Reject;
    std::vector<FixtureBlock> blocks{{}};
};
struct FixturePlacement {
    std::size_t group = 0;
    unsigned fixture = 0;
    Address start;
    std::array<Address, 4> channels;
};
inline std::vector<FixturePlacement> fixture_layout(const std::vector<FixtureGroup> &groups) {
    if (groups.size() > 64)
        throw std::invalid_argument("At most 64 fixture groups");
    std::set<std::string> names;
    std::set<Address> occupied;
    std::vector<FixturePlacement> result;
    const std::string logical = "RGBW";
    for (std::size_t g = 0; g < groups.size(); ++g) {
        const auto &group = groups[g];
        if (!valid_utf8(group.name) || group.name.empty() || group.name.size() > 128 ||
            group.name.find_first_of("\r\n") != std::string::npos ||
            group.name.find('\0') != std::string::npos ||
            group.name.find_first_not_of(" \t") == std::string::npos)
            throw std::invalid_argument(
                "Group names require 1..128 UTF-8 bytes without line breaks");
        if (!names.insert(group.name).second)
            throw std::invalid_argument("Duplicate fixture group name: " + group.name);
        auto order = group.order;
        std::sort(order.begin(), order.end());
        if (order != "BGRW" || group.stride < 4 || group.stride > 512 ||
            group.boundary != FixtureBoundary::Reject)
            throw std::invalid_argument(
                group.name + ": use an RGBW permutation, stride 4..512 and Reject boundary policy");
        if (group.blocks.empty() || group.blocks.size() > 64)
            throw std::invalid_argument(group.name + ": require 1..64 address blocks");
        unsigned fixture = 0;
        for (const auto &block : group.blocks) {
            if (block.universe < 1 || block.universe > 32768 || block.address < 1 ||
                block.address > 509 || block.count < 1 || block.count > 512 ||
                result.size() + block.count > 512)
                throw std::invalid_argument(group.name +
                                            ": invalid block or more than 512 fixtures");
            // All operands are bounded above: no wrap, splitting or implicit universe rollover.
            if (block.address + (block.count - 1) * group.stride + 3 > 512)
                throw std::invalid_argument(
                    group.name +
                    ": block crosses channel 512; split it into explicit universe blocks");
            for (unsigned n = 0; n < block.count; ++n) {
                FixturePlacement p;
                p.group = g;
                p.fixture = ++fixture;
                p.start = {std::uint16_t(block.universe),
                           std::uint16_t(block.address + n * group.stride)};
                for (unsigned channel = 0; channel < 4; ++channel) {
                    Address address{p.start.universe, std::uint16_t(p.start.channel + channel)};
                    if (!occupied.insert(address).second)
                        throw std::invalid_argument(group.name + ": fixture collision at " +
                                                    std::to_string(address.universe) + "." +
                                                    std::to_string(address.channel));
                    p.channels[logical.find(group.order[channel])] = address;
                }
                result.push_back(p);
            }
        }
    }
    return result;
}
inline EncoderPatch compile_fixture_patch(const std::vector<FixtureGroup> &groups,
                                          EncoderPatch manual,
                                          const std::vector<Assignment> &reserved = {}) {
    std::set<Address> occupied;
    for (const auto &patch : manual) {
        validate(patch);
        for (const auto &a : patch)
            occupied.insert(a.address); // Legacy manual HTP overlap remains legal.
    }
    for (const auto &a : reserved)
        occupied.insert(a.address);
    for (const auto &fixture : fixture_layout(groups))
        for (unsigned color = 0; color < 4; ++color) {
            const auto address = fixture.channels[color];
            if (occupied.count(address))
                throw std::invalid_argument(
                    groups[fixture.group].name + ": collision with manual encoder/fader/cue at " +
                    std::to_string(address.universe) + "." + std::to_string(address.channel));
            manual[color].push_back({address, 255});
        }
    for (const auto &patch : manual)
        validate(patch); // Includes the combined 512-address per-encoder capacity.
    return manual;
}
} // namespace brasscue
