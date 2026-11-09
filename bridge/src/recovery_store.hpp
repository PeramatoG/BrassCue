// BrassCue - GPL-3.0. Two-generation durable storage with one writer and validated recovery.
#pragma once
#include "session_snapshot.hpp"
#include <filesystem>
#include <memory>
#include <optional>
namespace brasscue {
struct RecoveryLoad {
    std::optional<SessionSnapshot> snapshot;
    std::string warning;
    bool backup_used = false;
    bool needs_repair = false;
};
class RecoveryStore {
  public:
    explicit RecoveryStore(std::filesystem::path directory);
    ~RecoveryStore();
    RecoveryLoad load() const;
    void save(const SessionSnapshot &);
    const std::string &write_error() const {
        return write_error_;
    }

  private:
    struct Lock;
    std::filesystem::path directory_;
    std::unique_ptr<Lock> lock_;
    std::string write_error_;
};
} // namespace brasscue
