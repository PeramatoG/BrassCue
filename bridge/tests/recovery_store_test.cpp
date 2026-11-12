// BrassCue - GPL-3.0. Durable generations, corruption fallback, failed writes and bounded autosave.
#include "../src/controller_session.hpp"
#include "../src/profile_io.hpp"
#include "../src/session_autosave.hpp"
#include "check.hpp"
#include <chrono>
#include <fstream>
namespace {
using namespace brasscue;
template <class F> void Reject(F action) {
    bool failed = false;
    try {
        action();
    } catch (const std::exception &) {
        failed = true;
    }
    CHECK(failed);
}
void Put(const std::filesystem::path &path, const std::string &data) {
    std::ofstream(path, std::ios::binary) << data;
}
std::string Read(const std::filesystem::path &path) {
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), {}};
}
} // namespace
int main() {
    using namespace brasscue;
    const auto root = std::filesystem::temp_directory_path() /
                      ("brasscue-recovery-" +
                       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(root);
    auto portable = root / "manual profile.bcue";
    ControlProfile on_disk;
    save_profile(portable, on_disk);
    const auto original = Read(portable);
    ControllerSession session;
    auto p = session.profile();
    p.name = "Unsaved fixture patch";
    p.fixture_groups = {{"HOUSE", "RGBW", 4, FixtureBoundary::Reject, {{2, 469, 11}}}};
    p.network_interface_id = "unavailable-saved-adapter";
    session.configure(p);
    session.encoder(0, 93);
    session.fader(37);
    SessionSnapshot a{session.profile(), session.checkpoint(0), {portable.u8string(), true}};
    auto b = a;
    b.runtime.values.master = 99;
    auto c = a;
    c.runtime.values.master = 111;
    {
        RecoveryStore store(root / "recovery");
        CHECK(!store.load().snapshot && store.load().warning.empty());
        store.save(a);
        CHECK(store.load().snapshot->runtime.values.master == 37);
        CHECK(std::filesystem::exists(root / "recovery/previous.bcsession"));
        store.save(b);
        CHECK(store.load().snapshot->runtime.values.master == 99);
        CHECK(parse_session(Read(root / "recovery/previous.bcsession")).runtime.values.master ==
              37);
        {
            RecoveryStore other(root / "recovery");
            CHECK(!other.write_error().empty() && other.load().snapshot);
            Reject([&] { other.save(c); });
        }
        Put(root / "recovery/current.bcsession", "corrupt latest checkpoint");
        auto fallback = store.load();
        CHECK(fallback.backup_used && !fallback.warning.empty() &&
              fallback.snapshot->runtime.values.master == 37);
        store.save(c); // Never replace a good previous generation with a corrupt current one.
        CHECK(parse_session(Read(root / "recovery/previous.bcsession")).runtime.values.master ==
              37);
        Put(root / "recovery/current.tmp", "partial write left by a terminated process");
        CHECK(store.load().snapshot->runtime.values.master == 111);
        Put(root / "recovery/current.bcsession", "BRASSCUE_SESSION 99\n");
        CHECK(store.load().backup_used);
        std::filesystem::remove(root / "recovery/current.bcsession");
        CHECK(store.load().backup_used);
        Put(root / "recovery/previous.bcsession", "also corrupt");
        CHECK(!store.load().snapshot && !store.load().warning.empty());
        store.save(c);
        const auto valid = Read(root / "recovery/current.bcsession");
        std::filesystem::create_directories(root / "recovery/current.tmp");
        Put(root / "recovery/current.tmp/blocker", "failed write injection");
        Reject([&] { store.save(b); });
        CHECK(Read(root / "recovery/current.bcsession") == valid);
        CHECK(store.load().snapshot->runtime.values.master == 111);
        std::filesystem::remove_all(root / "recovery/current.tmp");
        store.save(b);
        // Replacement failure after preparing the next checkpoint must retain a valid older file.
        std::filesystem::remove(root / "recovery/current.bcsession");
        std::filesystem::create_directories(root / "recovery/current.bcsession");
        Put(root / "recovery/current.bcsession/blocker", "not a replaceable file");
        Reject([&] { store.save(a); });
        CHECK(store.load().backup_used && store.load().snapshot->runtime.values.master == 111);
    }
    CHECK(Read(portable) == original); // Autosave never writes the associated portable document.
    {
        SessionAutosave autosave(root / "schedule");
        CHECK(!autosave.load().snapshot);
        autosave.poll(a, 0);
        CHECK(autosave.writes() == 0 && !autosave.due(40));
        autosave.poll(a, 250);
        CHECK(autosave.writes() == 0);
        autosave.poll(a, 500);
        CHECK(autosave.writes() == 1);
        for (unsigned i = 1; i <= 10; ++i) {
            a.runtime.values.rgbw[0] = std::uint8_t(i);
            autosave.poll(a, 500 + i * 250);
        }
        CHECK(autosave.writes() ==
              2); // Sustained movement checkpoints at two seconds, never 25 Hz.
        autosave.poll(a, 3250);
        autosave.poll(a, 3500);
        CHECK(autosave.writes() == 3);
        autosave.poll(a, 10000);
        CHECK(autosave.writes() == 3);
        a.document.dirty = false;
        CHECK(autosave.flush(a) && autosave.writes() == 4);
        CHECK(autosave.flush(a) && autosave.writes() == 4);
        RecoveryStore reader(root / "schedule");
        CHECK(!reader.load().snapshot->document.dirty);
        CHECK(reader.load().snapshot->profile.network_interface_id == "unavailable-saved-adapter");
    }
    {
        SessionAutosave failing(root / "retry");
        failing.load();
        std::filesystem::create_directories(root / "retry/current.tmp");
        Put(root / "retry/current.tmp/blocker", "write failure");
        failing.poll(a, 0);
        failing.poll(a, 500);
        CHECK(failing.writes() == 0 && !failing.error().empty());
        std::filesystem::remove_all(root / "retry/current.tmp");
        failing.poll(a, 1000);
        CHECK(failing.writes() == 0);
        failing.poll(a, 6000);
        CHECK(failing.writes() == 1 && failing.error().empty());
    }
    {
        // A stale temp with no valid generations never becomes an unvalidated session.
        std::filesystem::create_directories(root / "partial");
        Put(root / "partial/current.tmp", serialize_session(a).substr(0, 70));
        RecoveryStore partial(root / "partial");
        CHECK(!partial.load().snapshot && !partial.load().warning.empty());
    }
    {
        RecoveryStore seed(root / "repair");
        seed.save(b);
    }
    Put(root / "repair/previous.bcsession", "damaged redundant copy");
    {
        SessionAutosave repair(root / "repair");
        const auto data = repair.load();
        CHECK(data.snapshot && data.needs_repair && !data.backup_used);
        repair.poll(*data.snapshot, 0);
        repair.poll(*data.snapshot, 500);
        CHECK(repair.writes() == 1);
        CHECK(parse_session(Read(root / "repair/previous.bcsession")).runtime.values.master == 99);
    }
    CHECK(Read(portable) == original);
    std::filesystem::remove_all(root);
}
