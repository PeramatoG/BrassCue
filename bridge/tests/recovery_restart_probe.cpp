// BrassCue - GPL-3.0. Process-boundary probe: _Exit bypasses all destructors and normal shutdown.
#include "../src/controller_session.hpp"
#include "../src/output_session.hpp"
#include "../src/session_autosave.hpp"
#include "check.hpp"
#include <cstdlib>
#include <iostream>
int main(int argc, char **argv) {
    CHECK(argc == 3);
    const auto directory = std::filesystem::u8path(argv[2]);
    brasscue::SessionAutosave autosave(directory);
    auto loaded = autosave.load();
    if (std::string(argv[1]) == "write-crash") {
        brasscue::ControllerSession s;
        auto p = s.profile();
        p.name = "Unsaved after crash";
        p.fixture_groups = {
            {"Recovered group", "RGBW", 4, brasscue::FixtureBoundary::Reject, {{2, 469, 11}}}};
        s.configure(p);
        s.fader(137);
        s.encoder(0, 222);
        brasscue::SessionSnapshot data{p, s.checkpoint(0), {{}, true}};
        autosave.poll(data, 0);
        autosave.poll(data, 500);
        CHECK(autosave.writes() == 1);
        s.encoder(0, 42); // Not checkpointed: must not magically replace the durable state.
        std::_Exit(17);
    }
    CHECK(autosave.error().empty()); // A crashed writer's OS lock must actually be reacquired.
    CHECK(loaded.snapshot && loaded.snapshot->document.dirty);
    CHECK(loaded.snapshot->profile.name == "Unsaved after crash");
    brasscue::ControllerSession s;
    s.restore(loaded.snapshot->profile, loaded.snapshot->runtime);
    CHECK(s.state().rgbw[0] == 222 && s.state().master == 137);
    CHECK(s.engine().render(0)[2][468] == 119);
    if (std::string(argv[1]) == "read-send") {
        brasscue::OutputSession output;
        CHECK(!output.enabled());
        output.enable("127.0.0.1"); // Explicit opt-in after restoration, never startup behavior.
        output.send(s.engine(), 0);
        output.stop();
    }
    CHECK(autosave.flush(*loaded.snapshot)); // Acquires the writer lock after abrupt process death.
    std::cout << "Recovered configuration, levels, derived patch and writer lock\n";
}
