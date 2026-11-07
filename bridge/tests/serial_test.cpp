#include "../src/serial_protocol.hpp"
#include <iostream>

namespace {
int failures = 0;
void expect(bool condition, const char* name) {
    if (!condition) { std::cerr << "FAILED: " << name << '\n'; ++failures; }
}
}

int main() {
    using namespace brasscue;
    DeviceEvent event;
    expect(parse_device_message("BC1 READY", event) && event.type == EventType::Ready, "ready");
    expect(parse_device_message("BC1 ENC 0 1", event) && event.type == EventType::Encoder && event.index == 0 && event.value == 1, "encoder");
    State state;
    expect(apply_event(state, event, false) && state.rgbw[0] == 1, "increment");
    expect(parse_device_message("BC1 ENC 3 -1", event) && event.index == 3, "reverse");
    expect(!apply_event(state, event, false), "underflow clamps");
    state.rgbw[3] = 255;
    expect(!apply_event(state, DeviceEvent{EventType::Encoder, 3, 1}, false), "overflow clamps");
    expect(parse_device_message("BC1 BTN 2 1", event) && event.type == EventType::Button && event.value == 1, "button");
    expect(!apply_event(state, event, false), "button not bound");
    expect(parse_device_message("BC1 GM 42", event) && event.value == 42, "grandmaster");
    expect(!apply_event(state, event, true) && state.master == 255, "ignore floating gm");
    expect(apply_event(state, event, false) && state.master == 42, "accept gm");
    expect(!apply_event(state, event, false), "unchanged gm");
    expect(parse_device_message("BC1 ACK STATE", event) && event.type == EventType::StateAck, "ack");
    expect(!parse_device_message("GM 57", event), "old diagnostics rejected");
    expect(!parse_device_message("BC2 READY", event), "unknown protocol version");
    expect(!parse_device_message("BC1 ENC 4 1", event), "encoder invalid index");
    expect(!parse_device_message("BC1 ENC 0 9", event), "encoder invalid delta");
    expect(!parse_device_message("BC1 GM -1", event), "negative gm");
    expect(!parse_device_message("BC1 GM 256", event), "overflow gm");
    expect(!parse_device_message("BC1 GM 1 garbage", event), "extra fields");
    expect(!parse_device_message(std::string(200, 'x'), event), "long message");
    expect(serialize_state(state) == "BC1 STATE 1 0 0 255 42\n", "serialize state");
    if (failures == 0) std::cout << "BrassCue serial protocol tests passed\n";
    return failures ? 1 : 0;
}
