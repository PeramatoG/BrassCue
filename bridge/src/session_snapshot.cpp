// BrassCue - GPL-3.0. Bounded, checksummed, strict recovery records; no timestamps or output ON
// flag.
#include "session_snapshot.hpp"
#include "profile_io.hpp"
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
namespace brasscue {
namespace {
std::uint32_t Checksum(const std::string &data) {
    std::uint32_t crc = 0xffffffffu;
    for (unsigned char byte : data) {
        crc ^= byte;
        for (unsigned bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
    }
    return ~crc;
}
unsigned Number(std::istream &in, unsigned max) {
    std::string token;
    unsigned value = 0;
    if (!(in >> token) || token.empty())
        throw std::invalid_argument("Missing session number");
    for (char c : token) {
        if (c < '0' || c > '9' || unsigned(c - '0') > max || value > (max - unsigned(c - '0')) / 10)
            throw std::invalid_argument("Invalid session number");
        value = value * 10 + unsigned(c - '0');
    }
    return value;
}
void EndLine(std::istream &in) {
    if (in.get() != '\n')
        throw std::invalid_argument("Invalid session record boundary");
}
void Document(const ProfileDocument &d) {
    if (d.path.size() > 16384 || !valid_utf8(d.path) || d.path.find('\0') != std::string::npos)
        throw std::invalid_argument("Invalid recovered profile path");
}
} // namespace
std::string serialize_session(const SessionSnapshot &s) {
    validate(s.profile);
    validate_runtime(s.profile, s.runtime);
    Document(s.document);
    const auto profile = serialize_profile(s.profile);
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << profile.size() << '\n' << profile;
    out << unsigned(s.runtime.values.master);
    for (auto value : s.runtime.values.rgbw)
        out << ' ' << unsigned(value);
    out << ' ' << unsigned(s.runtime.patch_fader) << '\n';
    for (bool selected : s.runtime.internal.selected)
        out << selected << ' ';
    for (int owner : s.runtime.internal.owners)
        out << owner << ' ';
    for (bool held : s.runtime.internal.held)
        out << held << ' ';
    out << '\n' << std::setprecision(std::numeric_limits<double>::max_digits10);
    for (unsigned i = 0; i < 3; ++i)
        out << s.runtime.dmx.selected[i] << ' ' << s.runtime.dmx.levels[i] << ' ';
    out << '\n' << std::quoted(s.document.path) << ' ' << s.document.dirty << '\n';
    const auto payload = out.str();
    return "BRASSCUE_SESSION 1\n" + std::to_string(payload.size()) + ' ' +
           std::to_string(Checksum(payload)) + '\n' + payload;
}
SessionSnapshot parse_session(const std::string &text) {
    if (text.size() > 300000)
        throw std::invalid_argument("Session snapshot too large");
    std::istringstream header(text);
    header.imbue(std::locale::classic());
    std::string magic;
    if (!(header >> magic) || magic != "BRASSCUE_SESSION" || Number(header, 1) != 1)
        throw std::invalid_argument("Unsupported session version");
    EndLine(header);
    const auto size = Number(header, 300000), crc = Number(header, 0xffffffffu);
    EndLine(header);
    std::string payload(size, '\0');
    header.read(payload.data(), size);
    if (unsigned(header.gcount()) != size || header.peek() != std::char_traits<char>::eof() ||
        Checksum(payload) != crc)
        throw std::invalid_argument("Incomplete or corrupt session snapshot");
    std::istringstream in(payload);
    in.imbue(std::locale::classic());
    const auto profile_size = Number(in, 262144);
    EndLine(in);
    std::string profile(profile_size, '\0');
    in.read(profile.data(), profile_size);
    if (unsigned(in.gcount()) != profile_size)
        throw std::invalid_argument("Missing recovered profile");
    SessionSnapshot s;
    s.profile = parse_profile(profile);
    s.runtime.values.master = std::uint8_t(Number(in, 255));
    for (auto &value : s.runtime.values.rgbw)
        value = std::uint8_t(Number(in, 255));
    s.runtime.patch_fader = std::uint8_t(Number(in, 255));
    for (auto &selected : s.runtime.internal.selected)
        selected = Number(in, 1) != 0;
    for (auto &owner : s.runtime.internal.owners) {
        in >> std::ws;
        if (in.peek() == '-') {
            std::string token;
            in >> token;
            if (token != "-1")
                throw std::invalid_argument("Invalid recovered owner");
            owner = -1;
        } else
            owner = int(Number(in, 2));
    }
    for (auto &held : s.runtime.internal.held)
        held = Number(in, 1) != 0;
    for (unsigned i = 0; i < 3; ++i) {
        s.runtime.dmx.selected[i] = Number(in, 1) != 0;
        std::string token;
        if (!(in >> token) || token.size() > 64)
            throw std::invalid_argument("Invalid recovered cue level");
        std::istringstream number(token);
        number.imbue(std::locale::classic());
        if (!(number >> s.runtime.dmx.levels[i]) || number.peek() != std::char_traits<char>::eof())
            throw std::invalid_argument("Invalid recovered cue level");
    }
    in >> std::ws;
    if (in.peek() != '"' || !(in >> std::quoted(s.document.path)))
        throw std::invalid_argument("Missing recovered profile document");
    s.document.dirty = Number(in, 1) != 0;
    in >> std::ws;
    if (!in.eof())
        throw std::invalid_argument("Trailing session data");
    validate_runtime(s.profile, s.runtime);
    Document(s.document);
    return s;
}
} // namespace brasscue
