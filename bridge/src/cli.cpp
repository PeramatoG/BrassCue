// BrassCue - GPL-3.0. See LICENSE for terms and warranty disclaimer.
#include "bridge_core.hpp"
#include "controller_session.hpp"
#include "output_session.hpp"
#include "profile_io.hpp"
#include "serial_session.hpp"
#include <condition_variable>
#include <mutex>
#include <thread>
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

using brasscue::OutputSession;
std::uint64_t now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}
brasscue::ControlProfile cli_profile(std::uint16_t wire) {
    brasscue::ControlProfile p;
    for (auto &patch : p.encoders)
        for (auto &a : patch)
            a.address.universe = wire + 1;
    for (auto &button : p.buttons)
        for (auto &a : button.assignments)
            a.address.universe = wire + 1;
    return p;
}

class RefreshLoop {
  public:
    RefreshLoop(OutputSession *output, brasscue::ControllerSession &session)
        : worker_([this, output, &session] {
              std::unique_lock<std::mutex> lock(mutex);
              while (!stop_) {
                  session.advance(now_ms());
                  if (output)
                      try {
                          output->send(session.engine(), now_ms());
                      } catch (const std::exception &e) {
                          std::cerr << "Art-Net refresh failed: " << e.what() << "\n";
                          try {
                              output->stop();
                          } catch (...) {
                          }
                      }
                  cv_.wait_for(lock, std::chrono::milliseconds(40), [this] { return stop_; });
              }
          }) {}
    ~RefreshLoop() {
        {
            std::lock_guard<std::mutex> lock(mutex);
            stop_ = true;
        }
        cv_.notify_all();
        worker_.join();
    }
    std::mutex mutex;

  private:
    bool stop_ = false;
    std::condition_variable cv_;
    std::thread worker_;
};

unsigned parse_uint(const std::string &text, unsigned max) {
    std::size_t end = 0;
    const auto number = std::stoul(text, &end, 0);
    if (end != text.size() || number > max)
        throw std::invalid_argument("Value out of range: " + text);
    return static_cast<unsigned>(number);
}

void show(const brasscue::ControllerSession &session) {
    const auto &state = session.state();
    const auto frames = session.engine().render(now_ms());
    std::cout << "Raw RGBW:";
    for (auto value : state.rgbw)
        std::cout << " " << int(value);
    std::cout << (session.profile().fader.grandmaster ? " | Grandmaster: " : " | Patch fader: ")
              << int(session.fader_value()) << " | Output RGBW:";
    // For a fan-out patch, monitor the first address per encoder. Values come
    // from the final merged frame, including target scaling, cues and fader role.
    for (const auto &patch : session.encoder_patch()) {
        unsigned value = 0;
        if (!patch.empty()) {
            const auto address = patch.front().address;
            value = frames.at(address.universe).at(address.channel - 1);
        }
        std::cout << " " << value;
    }
    std::cout << "\n";
}

void usage() {
    std::cout << "BrassCue Bridge - simulated controls (no Arduino required)\n"
              << "Usage: brasscue-bridge [--simulate | --port COM3] [--ignore-gm] [--target IPv4] "
                 "[--universe 0..32767]\n"
              << "No network traffic unless --target is specified. UDP Art-Net port 6454.\n"
              << "Commands: r/g/b/w/m <0..255> (0xFF also works), show, send, btn <0..2> <0|1>, "
                 "load <path>, help, quit\n";
}

#ifdef _WIN32
int run_serial(const std::string &port, OutputSession *udp, const std::string &target,
               std::uint16_t universe, bool ignore_gm) {
    brasscue::SerialPort serial(port);
    brasscue::ControllerSession session;
    session.configure(cli_profile(universe));
    std::cout << "BrassCue USB bridge on " << port
              << " (115200 baud). Close Arduino Serial Monitor. Press Q to quit.\n";
    std::cout << (ignore_gm ? "Floating grandmaster input ignored.\n"
                            : "Grandmaster input enabled.\n");
    // The Uno announces READY after resetting when the COM port opens.
    // Only request HELLO if that spontaneous announcement never arrives.
    brasscue::SerialHandshake handshake(now_ms());
    std::string line, last_state;
    const auto send_state = [&] {
        const auto message = brasscue::serialize_state(session.state());
        if (message != last_state) {
            serial.send(message);
            last_state = message;
        }
    };
    std::uint64_t last_output = 0;
    while (true) {
        if (handshake.hello_due(now_ms()))
            serial.send("BC1 HELLO\n");
        if (_kbhit()) {
            const int key = _getch();
            if (key == 'q' || key == 'Q')
                break;
        }
        while (serial.read_line(line)) {
            brasscue::DeviceEvent event;
            if (!brasscue::parse_device_message(line, event)) {
                // Old diagnostic messages and noise are intentionally ignored.
                continue;
            }
            if (event.type == brasscue::EventType::Ready) {
                if (!handshake.ready(now_ms()))
                    continue;
                session.release_all(now_ms());
                last_state.clear();
                send_state();
                std::cout << "Arduino ready: sent initial RGBW/master state.\n";
            } else if (event.type == brasscue::EventType::StateAck) {
                std::cout << "Arduino acknowledged state.\n";
            } else if (handshake.connected()) {
                if (event.type == brasscue::EventType::Button) {
                    std::cout << "Button " << event.index << (event.value ? " DOWN\n" : " UP\n");
                }
                if (session.event(event, ignore_gm, now_ms())) {
                    show(session);
                    send_state();
                    if (udp) {
                        udp->send(session.engine(), now_ms());
                        std::cout << "[ArtDmx sent to " << target << ":6454; port-address "
                                  << universe << "]\n";
                    }
                }
            }
        }
        if (now_ms() - last_output >= 40) {
            session.advance(now_ms());
            if (handshake.connected())
                send_state();
            if (udp)
                udp->send(session.engine(), now_ms());
            last_output = now_ms();
        }
        Sleep(5);
    }
    return 0;
}
#endif

} // namespace

int run_cli(int argc, char **argv) {
    try {
        std::string target;
        std::string serial_port;
        bool ignore_gm = false;
        std::uint16_t universe = 0;
        for (int i = 1; i < argc; ++i) {
            const std::string arg = argv[i];
            if (arg == "--help" || arg == "-h") {
                usage();
                return 0;
            }
            if (arg == "--simulate")
                continue;
            if (arg == "--ignore-gm") {
                ignore_gm = true;
                continue;
            }
            if (arg == "--port" && i + 1 < argc) {
                serial_port = argv[++i];
                continue;
            }
            if (arg == "--target" && i + 1 < argc) {
                target = argv[++i];
                continue;
            }
            if (arg == "--universe" && i + 1 < argc) {
                universe = static_cast<std::uint16_t>(parse_uint(argv[++i], 32767));
                continue;
            }
            throw std::invalid_argument("Unknown or incomplete argument: " + arg);
        }

        std::unique_ptr<OutputSession> udp;
        if (!target.empty()) {
            udp = std::make_unique<OutputSession>();
            udp->enable(target);
        }
        if (!serial_port.empty()) {
#ifdef _WIN32
            return run_serial(serial_port, udp.get(), target, universe, ignore_gm);
#else
            throw std::runtime_error("--port serial mode currently requires Windows");
#endif
        }

        brasscue::ControllerSession session;
        session.configure(cli_profile(universe));
        usage();
        show(session);

        auto emit = [&] {
            if (!udp) {
                std::cout << "[simulation only - network disabled]\n";
            } else {
                udp->send(session.engine(), now_ms());
                std::cout << "[sent ArtDmx to " << target << ":6454, port-address " << universe
                          << "]\n";
            }
        };

        RefreshLoop refresh(udp.get(), session);
        std::string line;
        while (std::cout << "> " && std::getline(std::cin, line)) {
            std::lock_guard<std::mutex> lock(refresh.mutex);
            std::istringstream input(line);
            std::string command, value, trailing;
            if (!(input >> command))
                continue;
            if (command == "quit" || command == "exit")
                break;
            if (command == "help") {
                usage();
                continue;
            }
            if (command == "show") {
                show(session);
                continue;
            }
            if (command == "load" && std::getline(input >> std::ws, value) && !value.empty()) {
                try {
                    session.configure(brasscue::load_profile(std::filesystem::u8path(value)));
                    emit();
                } catch (const std::exception &e) {
                    std::cout << "Profile error: " << e.what() << "\n";
                }
                continue;
            }
            if (command == "btn") {
                unsigned index = 0, down = 0;
                if ((input >> index >> down) && index < 3 && down < 2 && !(input >> trailing)) {
                    session.button(index, down != 0, now_ms());
                    emit();
                } else
                    std::cout << "Expected btn <0..2> <0|1>\n";
                continue;
            }
            if (command == "send") {
                emit();
                continue;
            }
            if (command.size() != 1 || std::string("rgbwm").find(command[0]) == std::string::npos ||
                !(input >> value) || (input >> trailing)) {
                std::cout << "Invalid command. Type help.\n";
                continue;
            }
            try {
                const auto v = static_cast<std::uint8_t>(parse_uint(value, 255));
                if (command == "m")
                    session.fader(v, now_ms());
                else {
                    const auto channel = std::string("rgbw").find(command[0]);
                    session.encoder(channel, v, now_ms());
                }
                show(session);
                emit();
            } catch (const std::exception &ex) {
                std::cout << "Invalid value: " << ex.what() << "\n";
            }
        }
        return 0;
    } catch (const std::exception &ex) {
        std::cerr << "BrassCue Bridge: " << ex.what() << "\n";
        return 1;
    }
}
