// BrassCue - GPL-3.0. Real OS input, including Windows vcpkg builds without wx UI simulation.
#pragma once
#include <wx/uiaction.h>
#if wxUSE_UIACTIONSIMULATOR
#define BRASSCUE_GUI_INPUT_SUPPORTED 1
using GuiInput = wxUIActionSimulator;
#elif defined(__WXMSW__)
#define BRASSCUE_GUI_INPUT_SUPPORTED 1
#include <windows.h>
class GuiInput {
  public:
    bool Char(int code, int modifiers = wxMOD_NONE) {
        WORD key = 0;
        switch (code) {
        case WXK_RETURN:
            key = VK_RETURN;
            break;
        case WXK_ESCAPE:
            key = VK_ESCAPE;
            break;
        case WXK_TAB:
            key = VK_TAB;
            break;
        case WXK_UP:
            key = VK_UP;
            break;
        case WXK_DOWN:
            key = VK_DOWN;
            break;
        case WXK_PAGEUP:
            key = VK_PRIOR;
            break;
        case WXK_PAGEDOWN:
            key = VK_NEXT;
            break;
        case WXK_HOME:
            key = VK_HOME;
            break;
        case WXK_END:
            key = VK_END;
            break;
        default:
            key = WORD(code);
            break;
        }
        bool ok = true;
        if (modifiers & wxMOD_CONTROL)
            ok = SendKey(VK_CONTROL, false) && ok;
        if (modifiers & wxMOD_SHIFT)
            ok = SendKey(VK_SHIFT, false) && ok;
        ok = SendKey(key, false) && ok;
        ok = SendKey(key, true) && ok;
        if (modifiers & wxMOD_SHIFT)
            ok = SendKey(VK_SHIFT, true) && ok;
        if (modifiers & wxMOD_CONTROL)
            ok = SendKey(VK_CONTROL, true) && ok;
        return ok;
    }
    bool Text(const char *text) {
        for (; *text; ++text) {
            const SHORT key = VkKeyScanA(*text);
            if (key == -1 || !Char(LOBYTE(key), (HIBYTE(key) & 1) ? wxMOD_SHIFT : wxMOD_NONE))
                return false;
        }
        return true;
    }
    bool MouseMove(int x, int y) {
        return SetCursorPos(x, y) != 0;
    }
    bool MouseDown() {
        return Mouse(MOUSEEVENTF_LEFTDOWN);
    }
    bool MouseUp() {
        return Mouse(MOUSEEVENTF_LEFTUP);
    }

  private:
    static bool SendKey(WORD key, bool up) {
        INPUT input{};
        input.type = INPUT_KEYBOARD;
        input.ki.wVk = key;
        input.ki.dwFlags = up ? KEYEVENTF_KEYUP : 0;
        // Dedicated navigation keys are extended; without this flag wx receives
        // WXK_NUMPAD_END/HOME/etc. instead of the key being tested.
        if (key == VK_UP || key == VK_DOWN || key == VK_LEFT || key == VK_RIGHT || key == VK_HOME ||
            key == VK_END || key == VK_PRIOR || key == VK_NEXT)
            input.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
        return SendInput(1, &input, sizeof(input)) == 1;
    }
    static bool Mouse(DWORD flags) {
        INPUT input{};
        input.type = INPUT_MOUSE;
        input.mi.dwFlags = flags;
        return SendInput(1, &input, sizeof(input)) == 1;
    }
};
#else
#define BRASSCUE_GUI_INPUT_SUPPORTED 0
#endif
