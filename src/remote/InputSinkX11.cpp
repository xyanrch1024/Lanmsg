#include "remote/InputSinkX11.h"

#include <QDebug>
#include <QKeyEvent>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XTest.h>
#include <X11/keysym.h>

#include <cctype>

namespace {

// Qt::Key -> X11 keysym for non-ASCII (printable ASCII equals Qt::Key value == XK code).
unsigned long keyToKeysym(int key) {
    switch (key) {
    case Qt::Key_Escape: return XK_Escape;
    case Qt::Key_Tab: return XK_Tab;
    case Qt::Key_Backtab: return XK_ISO_Left_Tab;
    case Qt::Key_Return: return XK_Return;
    case Qt::Key_Enter: return XK_Return;
    case Qt::Key_Backspace: return XK_BackSpace;
    case Qt::Key_Delete: return XK_Delete;
    case Qt::Key_Insert: return XK_Insert;
    case Qt::Key_Home: return XK_Home;
    case Qt::Key_End: return XK_End;
    case Qt::Key_PageUp: return XK_Page_Up;
    case Qt::Key_PageDown: return XK_Page_Down;
    case Qt::Key_Left: return XK_Left;
    case Qt::Key_Right: return XK_Right;
    case Qt::Key_Up: return XK_Up;
    case Qt::Key_Down: return XK_Down;
    case Qt::Key_Shift: return XK_Shift_L;
    case Qt::Key_Control: return XK_Control_L;
    case Qt::Key_Alt: return XK_Alt_L;
    case Qt::Key_Meta: return XK_Super_L;
    case Qt::Key_CapsLock: return XK_Caps_Lock;
    case Qt::Key_NumLock: return XK_Num_Lock;
    case Qt::Key_ScrollLock: return XK_Scroll_Lock;
    case Qt::Key_Space: return XK_space;
    case Qt::Key_Print: return XK_Print;
    case Qt::Key_Pause: return XK_Pause;
    case Qt::Key_Menu: return XK_Menu;
    case Qt::Key_Super_L: return XK_Super_L;
    case Qt::Key_Super_R: return XK_Super_R;
    case Qt::Key_Help: return XK_Help;
    default:
        break;
    }
    if (key >= Qt::Key_F1 && key <= Qt::Key_F35)
        return XK_F1 + (key - Qt::Key_F1);
    if (key >= 0x20 && key <= 0x7e)
        return static_cast<unsigned long>(key);
    return 0; // unknown
}

} // namespace

InputSinkX11::InputSinkX11() = default;

InputSinkX11::~InputSinkX11() {
    shutdown();
}

bool InputSinkX11::initialize() {
    Display *dpy = XOpenDisplay(nullptr);
    if (!dpy) {
        qWarning() << "InputSinkX11: cannot open display";
        return false;
    }
    m_display = dpy;
    m_screen = DefaultScreen(dpy);
    return true;
}

void InputSinkX11::shutdown() {
    if (m_display) {
        XCloseDisplay(static_cast<Display *>(m_display));
        m_display = nullptr;
    }
}

void InputSinkX11::mouseMove(int x, int y) {
    if (!m_display)
        return;
    XTestFakeMotionEvent(static_cast<Display *>(m_display), m_screen, x, y, CurrentTime);
    XFlush(static_cast<Display *>(m_display));
}

void InputSinkX11::mouseButton(int button, bool down, int x, int y) {
    if (!m_display)
        return;
    if (x >= 0 && y >= 0)
        mouseMove(x, y);
    // button: 1=left 2=middle 3=right (X11 button numbers)
    XTestFakeButtonEvent(static_cast<Display *>(m_display), button, down ? True : False, CurrentTime);
    XFlush(static_cast<Display *>(m_display));
}

void InputSinkX11::wheel(int dx, int dy) {
    if (!m_display)
        return;
    Display *dpy = static_cast<Display *>(m_display);
    if (dy > 0)
        XTestFakeButtonEvent(dpy, 4, True, CurrentTime), XTestFakeButtonEvent(dpy, 4, False, CurrentTime);
    else if (dy < 0)
        XTestFakeButtonEvent(dpy, 5, True, CurrentTime), XTestFakeButtonEvent(dpy, 5, False, CurrentTime);
    if (dx > 0)
        XTestFakeButtonEvent(dpy, 7, True, CurrentTime), XTestFakeButtonEvent(dpy, 7, False, CurrentTime);
    else if (dx < 0)
        XTestFakeButtonEvent(dpy, 6, True, CurrentTime), XTestFakeButtonEvent(dpy, 6, False, CurrentTime);
    XFlush(dpy);
}

void InputSinkX11::key(int qtKey, bool down) {
    if (!m_display)
        return;
    const unsigned long sym = keyToKeysym(qtKey);
    if (!sym)
        return;
    Display *dpy = static_cast<Display *>(m_display);
    KeyCode code = XKeysymToKeycode(dpy, sym);
    if (code == 0)
        return;
    XTestFakeKeyEvent(dpy, code, down ? True : False, CurrentTime);
    XFlush(dpy);
}
