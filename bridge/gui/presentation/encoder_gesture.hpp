// BrassCue - GPL-3.0. Relative circular gesture in normalized screen coordinates.
#pragma once
#include <cmath>
namespace brasscue {
class EncoderGesture {
  public:
    void begin(double x, double y) {
        active_ = true;
        fraction_ = 0;
        anchor_ = usable(x, y);
        angle_ = std::atan2(y, x);
    }
    void end() {
        active_ = false;
        anchor_ = false;
        fraction_ = 0;
    }
    int move(double x, double y) {
        if (!active_)
            return 0;
        if (!usable(x, y)) {
            anchor_ = false;
            fraction_ = 0;
            return 0;
        }
        const auto angle = std::atan2(y, x);
        if (!anchor_) {
            angle_ = angle;
            anchor_ = true;
            return 0;
        }
        double delta = std::remainder(angle - angle_, 2 * pi);
        angle_ = angle;
        if (std::abs(delta) > pi / 2) {
            fraction_ = 0;
            return 0;
        } // Teleport: re-anchor, never jump.
        fraction_ += delta * 255 / (2 * pi);
        const int steps = static_cast<int>(fraction_);
        fraction_ -= steps;
        return steps;
    }

  private:
    static constexpr double pi = 3.14159265358979323846;
    static bool usable(double x, double y) {
        double r = x * x + y * y;
        return std::isfinite(r) && r >= 0.12 * 0.12 && r <= 3 * 3;
    }
    double angle_ = 0, fraction_ = 0;
    bool active_ = false, anchor_ = false;
};
} // namespace brasscue
