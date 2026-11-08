// BrassCue - GPL-3.0. Strict ordered text format, UTF-8 quoted labels.
#include "profile_io.hpp"
#include <fstream>
#include <iomanip>
#include <sstream>
#ifdef _WIN32
#include <windows.h>
#endif
namespace brasscue {
namespace {
void patch_out(std::ostream &out, const std::vector<Assignment> &patch) {
    out << patch.size();
    for (const auto &a : patch)
        out << ' ' << a.address.universe << ' ' << a.address.channel << ' ' << unsigned(a.value);
    out << '\n';
}
unsigned number(std::istream &in, unsigned max) {
    std::string token;
    unsigned value = 0;
    if (!(in >> token) || token.empty())
        throw std::invalid_argument("Missing profile number");
    for (char c : token) {
        if (c < '0' || c > '9' || unsigned(c - '0') > max || value > (max - unsigned(c - '0')) / 10)
            throw std::invalid_argument("Invalid profile number: " + token);
        value = value * 10 + unsigned(c - '0');
    }
    return value;
}
void label(std::istream &in, std::string &s) {
    in >> std::ws;
    if (in.peek() != '"' || !(in >> std::quoted(s)))
        throw std::invalid_argument("Expected quoted label");
}
void patch_in(std::istream &in, std::vector<Assignment> &patch) {
    const auto n = number(in, 512);
    patch.clear();
    for (unsigned i = 0; i < n; ++i) {
        const auto u = number(in, 32768), c = number(in, 512), v = number(in, 255);
        patch.push_back({{static_cast<std::uint16_t>(u), static_cast<std::uint16_t>(c)},
                         static_cast<std::uint8_t>(v)});
    }
    validate(patch);
}
} // namespace
std::string serialize_profile(const ControlProfile &p) {
    validate(p);
    std::ostringstream out;
    out << "BRASSCUE 2\n"
        << std::quoted(p.id) << ' ' << std::quoted(p.name) << ' ' << std::quoted(p.layout_id)
        << '\n';
    for (std::size_t i = 0; i < 4; ++i) {
        out << std::quoted(p.encoder_labels[i]) << ' ';
        patch_out(out, p.encoders[i]);
    }
    out << std::quoted(p.fader_label) << ' ' << p.fader.grandmaster << ' ';
    patch_out(out, p.fader.assignments);
    for (std::size_t i = 0; i < 3; ++i) {
        const auto &b = p.buttons[i];
        out << std::quoted(p.button_labels[i]) << ' ' << int(b.mode) << ' ' << b.kill << ' '
            << b.fade_in_ms << ' ' << b.fade_out_ms << ' ' << int(b.action) << ' ';
        patch_out(out, b.assignments);
        out << b.controls.size();
        for (const auto &a : b.controls)
            out << ' ' << int(a.target) << ' ' << unsigned(a.value);
        out << '\n';
    }
    out << std::quoted(p.screen_title) << ' ' << int(p.screen_view) << ' ' << int(p.units) << ' '
        << std::quoted(p.target_ip) << ' ' << p.allow_broadcast << ' '
        << std::quoted(p.network_interface_id) << '\n';
    return out.str();
}
ControlProfile parse_profile(const std::string &text) {
    if (text.size() > 262144)
        throw std::invalid_argument("Profile too large");
    std::istringstream in(text);
    std::string magic;
    if (!(in >> magic) || magic != "BRASSCUE")
        throw std::invalid_argument("Unsupported profile version");
    const auto version = number(in, 2);
    if (version < 1)
        throw std::invalid_argument("Unsupported profile version");
    ControlProfile p;
    label(in, p.id);
    label(in, p.name);
    label(in, p.layout_id);
    for (std::size_t i = 0; i < 4; ++i) {
        label(in, p.encoder_labels[i]);
        patch_in(in, p.encoders[i]);
    }
    label(in, p.fader_label);
    p.fader.grandmaster = number(in, 1) != 0;
    patch_in(in, p.fader.assignments);
    for (std::size_t i = 0; i < 3; ++i) {
        auto &b = p.buttons[i];
        b.action = ButtonActionKind::DmxCue;
        b.controls.clear();
        label(in, p.button_labels[i]);
        b.mode = static_cast<ButtonMode>(number(in, 1));
        b.kill = number(in, 1) != 0;
        b.fade_in_ms = number(in, 60000);
        b.fade_out_ms = number(in, 60000);
        if (version >= 2)
            b.action = static_cast<ButtonActionKind>(number(in, 1));
        patch_in(in, b.assignments);
        if (version >= 2) {
            const auto n = number(in, 5);
            for (unsigned j = 0; j < n; ++j) {
                const auto target = number(in, 4), value = number(in, 255);
                b.controls.push_back(
                    {static_cast<ControlTarget>(target), static_cast<std::uint8_t>(value)});
            }
        }
    }
    label(in, p.screen_title);
    p.screen_view = static_cast<ScreenView>(number(in, 1));
    p.units = static_cast<ValueDisplay>(number(in, 1));
    label(in, p.target_ip);
    p.allow_broadcast = number(in, 1) != 0;
    if (version >= 2)
        label(in, p.network_interface_id);
    in >> std::ws;
    if (!in.eof())
        throw std::invalid_argument("Trailing profile data");
    validate(p);
    return p;
}
ControlProfile load_profile(const std::filesystem::path &path) {
    std::ifstream in(path, std::ios::binary);
    if (!in)
        throw std::runtime_error("Cannot open profile");
    std::string text;
    char c;
    while (in.get(c)) {
        text += c;
        if (text.size() > 262144)
            throw std::runtime_error("Profile too large");
    }
    if (!in.eof())
        throw std::runtime_error("Cannot read profile");
    return parse_profile(text);
}
void save_profile(const std::filesystem::path &path, const ControlProfile &p) {
    const auto text = serialize_profile(p);
    auto temp = path;
    temp += ".tmp";
    if (std::filesystem::exists(temp))
        throw std::runtime_error("Temporary profile file already exists");
    try {
        {
            std::ofstream out(temp, std::ios::binary);
            out.exceptions(std::ios::failbit | std::ios::badbit);
            out.write(text.data(), static_cast<std::streamsize>(text.size()));
            out.close();
        }
#ifdef _WIN32
        if (!MoveFileExW(temp.c_str(), path.c_str(),
                         MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            throw std::runtime_error("Cannot replace profile");
#else
        std::filesystem::rename(temp, path);
#endif
    } catch (...) {
        std::error_code ignored;
        std::filesystem::remove(temp, ignored);
        throw;
    }
}
} // namespace brasscue
