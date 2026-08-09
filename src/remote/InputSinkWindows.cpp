#include "remote/InputSinkWindows.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <QDebug>
#include <QKeyEvent>

namespace {

// Qt::Key -> Windows virtual-key code. Letters/digits share the same value as
// their VK code on every layout (physical key semantics, matches the X11 sink
// which sends a keycode derived from the keysym). Other printable characters
// are resolved through the current keyboard layout, best effort.
UINT keyToVk(int key) {
    switch (key) {
    case Qt::Key_Escape: return VK_ESCAPE;
    case Qt::Key_Tab: return VK_TAB;
    case Qt::Key_Backtab: return VK_TAB;
    case Qt::Key_Return: return VK_RETURN;
    case Qt::Key_Enter: return VK_RETURN;
    case Qt::Key_Backspace: return VK_BACK;
    case Qt::Key_Delete: return VK_DELETE;
    case Qt::Key_Insert: return VK_INSERT;
    case Qt::Key_Home: return VK_HOME;
    case Qt::Key_End: return VK_END;
    case Qt::Key_PageUp: return VK_PRIOR;
    case Qt::Key_PageDown: return VK_NEXT;
    case Qt::Key_Left: return VK_LEFT;
    case Qt::Key_Right: return VK_RIGHT;
    case Qt::Key_Up: return VK_UP;
    case Qt::Key_Down: return VK_DOWN;
    case Qt::Key_Shift: return VK_SHIFT;
    case Qt::Key_Control: return VK_CONTROL;
    case Qt::Key_Alt: return VK_MENU;
    case Qt::Key_Meta: return VK_LWIN;
    case Qt::Key_CapsLock: return VK_CAPITAL;
    case Qt::Key_NumLock: return VK_NUMLOCK;
    case Qt::Key_ScrollLock: return VK_SCROLL;
    case Qt::Key_Space: return VK_SPACE;
    case Qt::Key_Print: return VK_SNAPSHOT;
    case Qt::Key_Pause: return VK_PAUSE;
    case Qt::Key_Menu: return VK_APPS;
    case Qt::Key_Super_L: return VK_LWIN;
    case Qt::Key_Super_R: return VK_RWIN;
    case Qt::Key_Help: return VK_HELP;
    default:
        break;
    }
    if (key >= Qt::Key_F1 && key <= Qt::Key_F24)
        return VK_F1 + (key - Qt::Key_F1);
    if (key >= 0x30 && key <= 0x39)
        return static_cast<UINT>(key); // '0'-'9'
    if (key >= 0x41 && key <= 0x5a)
        return static_cast<UINT>(key); // 'A'-'Z'
    if (key >= 0x20 && key <= 0x7e) {
        const SHORT vk = VkKeyScanW(static_cast<WCHAR>(key));
        if (vk != -1)
            return static_cast<UINT>(LOBYTE(vk));
    }
    return 0; // unknown
}

} // namespace

InputSinkWindows::InputSinkWindows() = default;

InputSinkWindows::~InputSinkWindows() {
    shutdown();
}

bool InputSinkWindows::initialize() {
    m_initialized = true;
    return true;
}

void InputSinkWindows::shutdown() {
    m_initialized = false;
}

void InputSinkWindows::mouseMove(int x, int y) {
    if (!m_initialized)
        return;
    SetCursorPos(x, y);
}

void InputSinkWindows::mouseButton(int button, bool down, int x, int y) {
    if (!m_initialized)
        return;
    // button: 1=left 2=middle 3=right (matches X11 convention)
    DWORD flag = 0;
    switch (button) {
    case 1: flag = down ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP; break;
    case 2: flag = down ? MOUSEEVENTF_MIDDLEDOWN : MOUSEEVENTF_MIDDLEUP; break;
    case 3: flag = down ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP; break;
    default:
        return;
    }
    if (x >= 0 && y >= 0)
        SetCursorPos(x, y);
    INPUT in{};
    in.type = INPUT_MOUSE;
    in.mi.dwFlags = flag;
    SendInput(1, &in, sizeof(in));
}

void InputSinkWindows::wheel(int dx, int dy) {
    if (!m_initialized)
        return;
    if (dy) {
        INPUT in{};
        in.type = INPUT_MOUSE;
        in.mi.dwFlags = MOUSEEVENTF_WHEEL;
        in.mi.mouseData = static_cast<DWORD>(static_cast<short>(dy * WHEEL_DELTA));
        SendInput(1, &in, sizeof(in));
    }
    if (dx) {
        INPUT in{};
        in.type = INPUT_MOUSE;
        in.mi.dwFlags = MOUSEEVENTF_HWHEEL;
        in.mi.mouseData = static_cast<DWORD>(static_cast<short>(dx * WHEEL_DELTA));
        SendInput(1, &in, sizeof(in));
    }
}

void InputSinkWindows::key(int qtKey, bool down) {
    if (!m_initialized)
        return;
    const UINT vk = keyToVk(qtKey);
    if (!vk)
        return;
    INPUT in{};
    in.type = INPUT_KEYBOARD;
    in.ki.wVk = static_cast<WORD>(vk);
    in.ki.dwFlags = down ? 0 : KEYEVENTF_KEYUP;
    SendInput(1, &in, sizeof(in));
}
