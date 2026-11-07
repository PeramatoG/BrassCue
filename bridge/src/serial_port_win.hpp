#pragma once

#ifdef _WIN32
#include <windows.h>
#include <stdexcept>
#include <string>

namespace brasscue {

// Single-owner Windows COM port wrapper. Closing Arduino IDE's Serial Monitor
// is required before opening the port (Windows serial ports are exclusive).
class SerialPort {
public:
    explicit SerialPort(const std::string& port) {
        const std::string path = "\\\\.\\" + port;
        handle_ = CreateFileA(path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (handle_ == INVALID_HANDLE_VALUE)
            throw std::runtime_error("Cannot open serial port " + port + "; close Arduino Serial Monitor");
        DCB dcb{};
        dcb.DCBlength = sizeof(dcb);
        if (!GetCommState(handle_, &dcb)) fail("GetCommState");
        dcb.BaudRate = CBR_115200;
        dcb.ByteSize = 8;
        dcb.Parity = NOPARITY;
        dcb.StopBits = ONESTOPBIT;
        dcb.fOutxCtsFlow = FALSE;
        dcb.fOutxDsrFlow = FALSE;
        dcb.fDtrControl = DTR_CONTROL_ENABLE;
        dcb.fRtsControl = RTS_CONTROL_DISABLE;
        dcb.fOutX = FALSE;
        dcb.fInX = FALSE;
        if (!SetCommState(handle_, &dcb)) fail("SetCommState");
        COMMTIMEOUTS timeouts{};
        timeouts.ReadIntervalTimeout = MAXDWORD;
        timeouts.ReadTotalTimeoutMultiplier = 0;
        timeouts.ReadTotalTimeoutConstant = 0;
        timeouts.WriteTotalTimeoutConstant = 1000;
        if (!SetCommTimeouts(handle_, &timeouts)) fail("SetCommTimeouts");
        // The Uno resets on DTR. Firmware sends READY after startup, but an
        // explicit HELLO is also sent after the reset window.
        PurgeComm(handle_, PURGE_RXCLEAR | PURGE_TXCLEAR);
    }

    SerialPort(const SerialPort&) = delete;
    SerialPort& operator=(const SerialPort&) = delete;

    ~SerialPort() {
        if (handle_ != INVALID_HANDLE_VALUE) CloseHandle(handle_);
    }

    void send(const std::string& line) {
        DWORD written = 0;
        if (!WriteFile(handle_, line.data(), static_cast<DWORD>(line.size()), &written, nullptr) ||
            written != line.size()) throw std::runtime_error("Serial write failed; device disconnected?");
    }

    // Returns true only when a complete bounded ASCII line is available.
    bool read_line(std::string& output) {
        for (int i = 0; i < 256; ++i) {
            char c = 0;
            DWORD n = 0;
            if (!ReadFile(handle_, &c, 1, &n, nullptr))
                throw std::runtime_error("Serial read failed; device disconnected?");
            if (n == 0) return false;
            if (c == '\r') continue;
            if (c == '\n') {
                if (overflow_) { overflow_ = false; buffer_.clear(); continue; }
                if (buffer_.empty()) continue;
                output.swap(buffer_);
                buffer_.clear();
                return true;
            }
            if (overflow_) continue;
            if (c < 32 || c > 126 || buffer_.size() >= 80) {
                buffer_.clear();
                overflow_ = true;
                continue;
            }
            buffer_ += c;
        }
        return false;
    }

private:
    void fail(const char* action) {
        CloseHandle(handle_);
        handle_ = INVALID_HANDLE_VALUE;
        throw std::runtime_error(std::string(action) + " failed on serial port");
    }
    HANDLE handle_ = INVALID_HANDLE_VALUE;
    std::string buffer_;
    bool overflow_ = false;
};

} // namespace brasscue
#endif
