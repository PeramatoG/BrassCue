// BrassCue - GPL-3.0. Atomic replacement, fsync/FlushFileBuffers and a crash-released OS lock.
#include "recovery_store.hpp"
#include <fstream>
#ifdef _WIN32
#include <windows.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#endif
namespace brasscue {
namespace {
std::string Read(const std::filesystem::path &path) {
    if (!std::filesystem::is_regular_file(std::filesystem::symlink_status(path)))
        throw std::runtime_error("Session recovery entry is not a regular file");
    std::ifstream in(path, std::ios::binary);
    if (!in)
        throw std::runtime_error("Cannot read session recovery file");
    std::string result;
    char buffer[4096];
    while (in) {
        in.read(buffer, sizeof buffer);
        result.append(buffer, std::size_t(in.gcount()));
        if (result.size() > 300000)
            throw std::runtime_error("Session recovery file too large");
    }
    if (!in.eof())
        throw std::runtime_error("Session recovery read failed");
    return result;
}
bool Valid(const std::filesystem::path &path) {
    try {
        (void)parse_session(Read(path));
        return true;
    } catch (...) {
        return false;
    }
}
void Write(const std::filesystem::path &path, const std::string &data) {
    // Only fixed, app-local temporary names are removed. Never follow an abandoned temp symlink.
    std::error_code ec;
    std::filesystem::remove(path, ec);
#ifdef _WIN32
    HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                              FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
        throw std::runtime_error("Cannot create session temporary file");
    DWORD written = 0;
    const bool ok = WriteFile(file, data.data(), DWORD(data.size()), &written, nullptr) &&
                    written == data.size() && FlushFileBuffers(file);
    CloseHandle(file);
#else
    const int file = ::open(path.c_str(), O_CREAT | O_EXCL | O_WRONLY | O_CLOEXEC, 0600);
    if (file < 0)
        throw std::runtime_error("Cannot create session temporary file");
    std::size_t written = 0;
    while (written < data.size()) {
        const auto n = ::write(file, data.data() + written, data.size() - written);
        if (n < 0 && errno == EINTR)
            continue;
        if (n <= 0)
            break;
        written += std::size_t(n);
    }
    const bool ok = written == data.size() && ::fsync(file) == 0;
    const bool closed = ::close(file) == 0;
    if (!closed)
        throw std::runtime_error("Cannot close session temporary file");
#endif
    if (!ok)
        throw std::runtime_error("Cannot flush session temporary file");
    (void)parse_session(
        Read(path)); // Validate what reached disk before promoting either generation.
}
void Replace(const std::filesystem::path &from, const std::filesystem::path &to) {
#ifdef _WIN32
    if (!MoveFileExW(from.c_str(), to.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        throw std::runtime_error("Cannot replace session recovery file");
#else
    std::filesystem::rename(from, to);
    const int directory = ::open(to.parent_path().c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (directory < 0)
        throw std::runtime_error("Cannot open recovery directory for synchronization");
    const int result = ::fsync(directory), failure = errno;
    ::close(directory);
    if (result && failure != EINVAL && failure != ENOTSUP)
        throw std::runtime_error("Cannot synchronize recovery directory");
#endif
}
} // namespace
struct RecoveryStore::Lock {
#ifdef _WIN32
    HANDLE file = INVALID_HANDLE_VALUE;
    ~Lock() {
        if (file != INVALID_HANDLE_VALUE)
            CloseHandle(file);
    }
#else
    int file = -1;
    ~Lock() {
        if (file >= 0)
            ::close(file);
    }
#endif
};
RecoveryStore::RecoveryStore(std::filesystem::path directory)
    : directory_(std::move(directory)), lock_(new Lock) {
    try {
        std::filesystem::create_directories(directory_);
        const auto path = directory_ / "writer.lock";
#ifdef _WIN32
        lock_->file = CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                                  OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (lock_->file == INVALID_HANDLE_VALUE)
            throw std::runtime_error(
                "Session recovery is read-only: another instance or unavailable storage");
#else
        lock_->file = ::open(path.c_str(), O_CREAT | O_RDWR | O_CLOEXEC, 0600);
        if (lock_->file < 0 || ::flock(lock_->file, LOCK_EX | LOCK_NB))
            throw std::runtime_error(
                "Session recovery is read-only: another instance or unavailable storage");
#endif
    } catch (const std::exception &e) {
        write_error_ = e.what();
    }
}
RecoveryStore::~RecoveryStore() = default;
RecoveryLoad RecoveryStore::load() const {
    RecoveryLoad result;
    result.warning = write_error_;
    bool found = false;
    for (const auto &name : {"current.bcsession", "previous.bcsession"}) {
        const auto path = directory_ / name;
        std::error_code ec;
        if (!std::filesystem::exists(path, ec))
            continue;
        found = true;
        try {
            result.snapshot = parse_session(Read(path));
            result.backup_used = std::string(name) == "previous.bcsession";
            if (result.backup_used)
                result.warning = "Previous session snapshot restored; latest recovery was "
                                 "unavailable or invalid";
            return result;
        } catch (const std::exception &) {
        }
    }
    std::error_code ec;
    if (found || std::filesystem::exists(directory_ / "current.tmp", ec))
        result.warning = "Session recovery was invalid or incomplete; built-in defaults loaded";
    return result;
}
void RecoveryStore::save(const SessionSnapshot &snapshot) {
    if (!write_error_.empty())
        throw std::runtime_error(write_error_);
    const auto data = serialize_session(snapshot);
    const auto current = directory_ / "current.bcsession",
               previous = directory_ / "previous.bcsession";
    const auto temp = directory_ / "current.tmp", backup = directory_ / "previous.tmp";
    Write(temp, data);
    if (Valid(current)) {
        Write(backup, Read(current));
        Replace(backup, previous);
    } else if (!Valid(previous)) {
        Write(backup, data); // First checkpoint: retain two valid copies.
        Replace(backup, previous);
    }
    Replace(temp, current);
}
} // namespace brasscue
