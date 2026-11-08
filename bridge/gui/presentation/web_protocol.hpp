// BrassCue - GPL-3.0. Bounded, epoch-scoped local input protocol; no toolkit dependency.
#pragma once
#include "controller_actions.hpp"
#include "encoder_gesture.hpp"
#include <array>
#include <string>
namespace brasscue {
class WebInputBridge {
  public:
    explicit WebInputBridge(ControllerActions actions) : actions_(std::move(actions)) {}
    void reset(std::uint64_t epoch);
    bool accept(const std::string &record);
    bool ready() const {
        return ready_;
    }
    void release_inputs();
    void reconcile_mouse(bool down);

  private:
    ControllerActions actions_;
    std::array<EncoderGesture, 4> gestures_{};
    std::array<unsigned, 3> buttons_{};
    std::uint64_t epoch_ = 0, last_sequence_ = 0;
    bool ready_ = false;
};
bool allowed_panel_navigation(const std::string &url, const std::string &main_url);
} // namespace brasscue
