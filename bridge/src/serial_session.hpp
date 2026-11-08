// BrassCue - GPL-3.0. BC1 startup timing and bounded receive framing.
#pragma once
#include "serial_protocol.hpp"
namespace brasscue {
class BoundedSerialLine {
  public:
    bool feed(char c, std::string &output) {
        if (c == '\r')
            return false;
        if (c == '\n') {
            const bool complete = !overflow_ && !buffer_.empty();
            if (complete)
                output = buffer_;
            buffer_.clear();
            overflow_ = false;
            return complete;
        }
        if (!overflow_) {
            if (c < 32 || c > 126 || buffer_.size() >= 80) {
                buffer_.clear();
                overflow_ = true;
            } else
                buffer_ += c;
        }
        return false;
    }

  private:
    std::string buffer_;
    bool overflow_ = false;
};
class SerialHandshake {
  public:
    explicit SerialHandshake(std::uint64_t startup) : startup_(startup) {}
    bool hello_due(std::uint64_t now) {
        if (!ready_ && !hello_sent_ && now >= startup_ && now - startup_ >= 2500) {
            hello_sent_ = true;
            return true;
        }
        return false;
    }
    bool ready(std::uint64_t now) {
        if (ready_ && now >= last_ready_ && now - last_ready_ < 1000)
            return false;
        ready_ = true;
        last_ready_ = now;
        return true;
    }
    bool connected() const {
        return ready_;
    }

  private:
    std::uint64_t startup_, last_ready_ = 0;
    bool hello_sent_ = false, ready_ = false;
};
} // namespace brasscue
