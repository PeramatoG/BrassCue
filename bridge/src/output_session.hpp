// BrassCue - GPL-3.0. Output snapshots and retired-universe cleanup policy.
#pragma once
#include "artnet_udp.hpp"
#include "cue_engine.hpp"
#include <memory>
namespace brasscue {
using DmxFrames = std::map<std::uint16_t, std::array<std::uint8_t, 512>>;
inline auto frame_packet(std::uint16_t universe, const std::array<std::uint8_t, 512> &slots) {
    if (universe < 1 || universe > 32768)
        throw std::out_of_range("Universe");
    auto bytes = build_artdmx(State{}, static_cast<std::uint16_t>(universe - 1));
    std::copy(slots.begin(), slots.end(), bytes.begin() + kArtDmxHeader);
    return bytes;
}
class OutputSession {
  public:
    bool enabled() const {
        return bool(target_);
    }
    void enable(const std::string &ip, bool broadcast = false) {
        auto next = std::make_unique<UdpTarget>(ip, broadcast);
        stop();
        target_ = std::move(next);
    }
    template <class Send> void update(DmxFrames frames, Send send) {
        for (const auto &old : last_)
            if (!frames.count(old.first))
                send(frame_packet(old.first, {}));
        for (const auto &frame : frames)
            send(frame_packet(frame.first, frame.second));
        last_ = std::move(frames);
    }
    void send(const CueEngine &engine, std::uint64_t now) {
        if (!target_)
            return;
        update(engine.render(now), [this](const auto &packet) { target_->send(packet); });
    }
    void stop() {
        if (!target_)
            return;
        try {
            for (const auto &frame : last_)
                target_->send(frame_packet(frame.first, {}));
        } catch (...) {
            target_.reset();
            last_.clear();
            throw;
        }
        target_.reset();
        last_.clear();
    }
    ~OutputSession() {
        try {
            stop();
        } catch (...) {
        }
    }

  private:
    std::unique_ptr<UdpTarget> target_;
    DmxFrames last_;
};
} // namespace brasscue
