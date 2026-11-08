// BrassCue - GPL-3.0. Validate persisted UTF-8 labels without a GUI dependency.
#pragma once
#include <cstdint>
#include <string>
namespace brasscue {
inline bool valid_utf8(const std::string &text) {
    std::uint32_t code = 0, minimum = 0;
    unsigned remaining = 0;
    for (unsigned char c : text) {
        if (remaining) {
            if ((c & 0xc0) != 0x80)
                return false;
            code = (code << 6) | (c & 0x3f);
            if (--remaining == 0 &&
                (code < minimum || code > 0x10ffff || (code >= 0xd800 && code <= 0xdfff)))
                return false;
        } else if (c < 0x80) {
            if (c < 32 || c == 127)
                return false;
        } else if (c >= 0xc2 && c <= 0xdf) {
            remaining = 1;
            code = c & 0x1f;
            minimum = 0x80;
        } else if (c >= 0xe0 && c <= 0xef) {
            remaining = 2;
            code = c & 0x0f;
            minimum = 0x800;
        } else if (c >= 0xf0 && c <= 0xf4) {
            remaining = 3;
            code = c & 7;
            minimum = 0x10000;
        } else
            return false;
    }
    return remaining == 0;
}
} // namespace brasscue
