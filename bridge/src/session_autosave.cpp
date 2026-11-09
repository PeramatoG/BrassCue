// BrassCue - GPL-3.0. Poll at most four times per second; write after 500 ms quiet or two seconds
// dirty.
#include "session_autosave.hpp"
namespace brasscue {
RecoveryLoad SessionAutosave::load() {
    auto result = store_.load();
    if (result.snapshot && !result.backup_used)
        saved_ = serialize_session(*result.snapshot);
    error_ = store_.write_error();
    return result;
}
void SessionAutosave::poll(const SessionSnapshot &snapshot, std::uint64_t now) {
    if (!due(now))
        return;
    next_poll_ = now + 250;
    try {
        const auto data = serialize_session(snapshot);
        if (data == saved_) {
            pending_.clear();
            return;
        }
        if (pending_.empty())
            first_change_ = now;
        if (data != pending_) {
            pending_ = data;
            last_change_ = now;
        }
        if (now < retry_ || (now - last_change_ < 500 && now - first_change_ < 2000))
            return;
        if (!flush(snapshot))
            retry_ = now + 5000;
    } catch (const std::exception &e) {
        error_ = e.what();
        retry_ = now + 5000;
    }
}
bool SessionAutosave::flush(const SessionSnapshot &snapshot) {
    try {
        const auto data = serialize_session(snapshot);
        if (data != saved_) {
            store_.save(snapshot);
            saved_ = data;
            ++writes_;
        }
        pending_.clear();
        error_.clear();
        return true;
    } catch (const std::exception &e) {
        error_ = e.what();
        return false;
    }
}
} // namespace brasscue
