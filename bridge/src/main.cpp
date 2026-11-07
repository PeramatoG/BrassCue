#include "bridge_core.hpp"
#include "artnet_udp.hpp"
#include "serial_protocol.hpp"
#ifdef _WIN32
#include "serial_port_win.hpp"
#include <conio.h>
#endif

#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>


namespace {

using brasscue::UdpTarget;

unsigned parse_uint(const std::string& text, unsigned max) {
    std::size_t end = 0;
    const auto number = std::stoul(text, &end, 0);
    if (end != text.size() || number > max) throw std::invalid_argument("Value out of range: " + text);
    return static_cast<unsigned>(number);
}

void show(const brasscue::State& state) {
    std::cout << "Raw RGBW:";
    for (const auto value : state.rgbw) std::cout << " " << static_cast<int>(value);
    std::cout << " | Grandmaster: " << static_cast<int>(state.master) << " | Output RGBW:";
    for (const auto value : state.rgbw)
        std::cout << " " << static_cast<int>(brasscue::apply_master(value, state.master));
    std::cout << "\n";
}

void usage() {
    std::cout << "BrassCue Bridge - simulated controls (no Arduino required)\n"
              << "Usage: brasscue-bridge [--simulate | --port COM3] [--ignore-gm] [--target IPv4] [--universe 0..32767]\n"
              << "No network traffic unless --target is specified. UDP Art-Net port 6454.\n"
              << "Commands: r/g/b/w/m <0..255> (0xFF also works), show, send, help, quit\n";
}


#ifdef _WIN32
int run_serial(const std::string& port, UdpTarget* udp,
               const std::string& target, std::uint16_t universe, bool ignore_gm) {
    brasscue::SerialPort serial(port);
    brasscue::State state;
    std::cout << "BrassCue USB bridge on " << port
              << " (115200 baud). Close Arduino Serial Monitor. Press Q to quit.\n";
    std::cout << (ignore_gm ? "Floating grandmaster input ignored.\n"
                           : "Grandmaster input enabled.\n");
    // The Uno announces READY after resetting when the COM port opens.
    // Only request HELLO if that spontaneous announcement never arrives.
    using Clock = std::chrono::steady_clock;
    const auto startup = Clock::now();
    Clock::time_point last_ready{};
    bool hello_sent = false;
    bool ready = false;
    std::string line;
    while (true) {
        if (!ready && !hello_sent &&
            Clock::now() - startup >= std::chrono::milliseconds(2500)) {
            serial.send("BC1 HELLO\n");
            hello_sent = true;
        }
        if (_kbhit()) {
            const int key = _getch();
            if (key == 'q' || key == 'Q') break;
        }
        while (serial.read_line(line)) {
            brasscue::DeviceEvent event;
            if (!brasscue::parse_device_message(line, event)) {
                // Old diagnostic messages and noise are intentionally ignored.
                continue;
            }
            if (event.type == brasscue::EventType::Ready) {
                const auto now = Clock::now();
                // A startup READY and a near-simultaneous HELLO reply describe
                // the same session. Still resynchronize on a later real reset.
                if (ready && now - last_ready < std::chrono::milliseconds(1000))
                    continue;
                last_ready = now;
                ready = true;
                serial.send(brasscue::serialize_state(state));
                std::cout << "Arduino ready: sent initial RGBW/master state.\n";
            } else if (event.type == brasscue::EventType::StateAck) {
                std::cout << "Arduino acknowledged state.\n";
            } else if (ready) {
                if (event.type == brasscue::EventType::Button) {
                    std::cout << "Button " << event.index
                              << (event.value ? " DOWN\n" : " UP\n");
                } else if (brasscue::apply_event(state, event, ignore_gm)) {
                    show(state);
                    serial.send(brasscue::serialize_state(state));
                    if (udp) {
                        udp->send(brasscue::build_artdmx(state, universe));
                        std::cout << "[ArtDmx sent to " << target
                                  << ":6454; port-address " << universe << "]\n";
                    }
                }
            }
        }
        Sleep(5);
    }
    return 0;
}
#endif

} // namespace

int main(int argc, char** argv) {
    try {
        std::string target;
        std::string serial_port;
        bool ignore_gm = false;
        std::uint16_t universe = 0;
        for (int i = 1; i < argc; ++i) {
            const std::string arg = argv[i];
            if (arg == "--help" || arg == "-h") { usage(); return 0; }
            if (arg == "--simulate") continue;
            if (arg == "--ignore-gm") { ignore_gm = true; continue; }
            if (arg == "--port" && i + 1 < argc) { serial_port = argv[++i]; continue; }
            if (arg == "--target" && i + 1 < argc) { target = argv[++i]; continue; }
            if (arg == "--universe" && i + 1 < argc) {
                universe = static_cast<std::uint16_t>(parse_uint(argv[++i], 32767));
                continue;
            }
            throw std::invalid_argument("Unknown or incomplete argument: " + arg);
        }

        std::unique_ptr<UdpTarget> udp;
        if (!target.empty()) udp = std::make_unique<UdpTarget>(target);
        if (!serial_port.empty()) {
#ifdef _WIN32
            return run_serial(serial_port, udp.get(), target, universe, ignore_gm);
#else
            throw std::runtime_error("--port serial mode currently requires Windows");
#endif
        }

        brasscue::State state;
        usage();
        show(state);

        auto emit = [&] {
            if (!udp) {
                std::cout << "[simulation only - network disabled]\n";
            } else {
                udp->send(brasscue::build_artdmx(state, universe));
                std::cout << "[sent ArtDmx to " << target << ":6454, port-address "
                          << universe << "]\n";
            }
        };

        std::string line;
        while (std::cout << "> " && std::getline(std::cin, line)) {
            std::istringstream input(line);
            std::string command, value, trailing;
            if (!(input >> command)) continue;
            if (command == "quit" || command == "exit") break;
            if (command == "help") { usage(); continue; }
            if (command == "show") { show(state); continue; }
            if (command == "send") { emit(); continue; }
            if (command.size() != 1 || std::string("rgbwm").find(command[0]) == std::string::npos ||
                !(input >> value) || (input >> trailing)) {
                std::cout << "Invalid command. Type help.\n";
                continue;
            }
            try {
                const auto v = static_cast<std::uint8_t>(parse_uint(value, 255));
                if (command == "m") state.master = v;
                else {
                    const auto channel = std::string("rgbw").find(command[0]);
                    state.rgbw[channel] = v;
                }
                show(state);
                emit();
            } catch (const std::exception& ex) {
                std::cout << "Invalid value: " << ex.what() << "\n";
            }
        }
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "BrassCue Bridge: " << ex.what() << "\n";
        return 1;
    }
}
