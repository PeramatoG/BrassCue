// BrassCue - GPL-3.0. Shared application state and event boundary.
#pragma once
#include "controller_profile.hpp"
#include "cue_engine.hpp"
#include "serial_protocol.hpp"
namespace brasscue {
class ControllerSession {
public:
    ControllerSession() { configure(ControlProfile{}); }
    const ControlProfile& profile() const { return profile_; }
    const State& state() const { return state_; }
    const CueEngine& engine() const { return engine_; }
    void configure(ControlProfile profile) {
        validate(profile);
        CueEngine next({profile.buttons.begin(),profile.buttons.end()});
        next.configure_fader(profile.fader);
        next.set_fader(state_.master);
        for (std::size_t i=0;i<4;++i) next.set_encoder(i,profile.encoders[i],state_.rgbw[i]);
        profile_=std::move(profile);
        engine_=std::move(next); // Configuration commits once; cue state resets safely.
    }
    void encoder(std::size_t i, std::uint8_t value) {
        engine_.set_encoder(i,profile_.encoders.at(i),value);
        state_.rgbw.at(i)=value;
    }
    void fader(std::uint8_t value) { state_.master=value; engine_.set_fader(value); }
    void button(std::size_t i,bool down,std::uint64_t now) { engine_.button(i,down,now); }
    void release_all(std::uint64_t now) { engine_.release_all(now); }
    bool event(const DeviceEvent& e,bool ignore_gm,std::uint64_t now) {
        if (e.type==EventType::Button) {
            if (e.index<0 || e.index>=3 || e.value<0 || e.value>1) return false;
            button(e.index,e.value!=0,now); return true;
        }
        State proposed=state_;
        if (!apply_event(proposed,e,ignore_gm)) return false;
        if (e.type==EventType::Encoder) encoder(e.index,proposed.rgbw[e.index]);
        else fader(proposed.master);
        return true;
    }
private:
    ControlProfile profile_;
    State state_;
    CueEngine engine_;
};
}
