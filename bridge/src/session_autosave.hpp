// BrassCue - GPL-3.0. Debounced recovery checkpoints, bounded transition latency and failed-write
// retries.
#pragma once
#include "recovery_store.hpp"
namespace brasscue {
class SessionAutosave {
  public:
    explicit SessionAutosave(std::filesystem::path directory) : store_(std::move(directory)) {}
    RecoveryLoad load();
    bool due(std::uint64_t now) const {
        return now >= next_poll_;
    }
    void poll(const SessionSnapshot &, std::uint64_t now);
    bool flush(const SessionSnapshot &);
    const std::string &error() const {
        return error_;
    }
    unsigned writes() const {
        return writes_;
    }

  private:
    RecoveryStore store_;
    std::string saved_, pending_, error_;
    std::uint64_t next_poll_ = 0, first_change_ = 0, last_change_ = 0, retry_ = 0;
    unsigned writes_ = 0;
};
} // namespace brasscue
