#include "remote/ScreenSourceX11.h"

#include <QDebug>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XShm.h>

#include <sys/ipc.h>
#include <sys/shm.h>
#include <unistd.h>

ScreenSourceX11::ScreenSourceX11() = default;

ScreenSourceX11::~ScreenSourceX11() {
    shutdown();
}

bool ScreenSourceX11::initialize() {
    Display *dpy = XOpenDisplay(nullptr);
    if (!dpy) {
        qWarning() << "ScreenSourceX11: cannot open display";
        return false;
    }
    m_display = dpy;
    const int screen = DefaultScreen(dpy);
    m_width = DisplayWidth(dpy, screen);
    m_height = DisplayHeight(dpy, screen);
    if (m_width <= 0 || m_height <= 0) {
        XCloseDisplay(dpy);
        m_display = nullptr;
        return false;
    }
    if (!initShm())
        qWarning() << "ScreenSourceX11: XShm unavailable, falling back to XGetImage";
    return true;
}

bool ScreenSourceX11::initShm() {
    Display *dpy = static_cast<Display *>(m_display);
    const int screen = DefaultScreen(dpy);
    XShmSegmentInfo *info = new XShmSegmentInfo;
    XImage *img = XShmCreateImage(dpy, DefaultVisual(dpy, screen), DefaultDepth(dpy, screen),
                                  ZPixmap, nullptr, info, m_width, m_height);
    if (!img) {
        delete info;
        return false;
    }
    m_shmInfo = reinterpret_cast<long>(info);
    m_shmImage = reinterpret_cast<long>(img);
    info->shmid = shmget(IPC_PRIVATE, static_cast<size_t>(img->bytes_per_line) * static_cast<size_t>(img->height),
                         IPC_CREAT | 0600);
    if (info->shmid < 0) {
        XDestroyImage(img);
        m_shmImage = 0;
        delete info;
        m_shmInfo = 0;
        return false;
    }
    info->shmaddr = img->data = static_cast<char *>(shmat(info->shmid, nullptr, 0));
    info->readOnly = False;
    m_shmId = info->shmid;
    m_shmData = info->shmaddr;
    if (m_shmData == reinterpret_cast<void *>(-1)) {
        shmctl(info->shmid, IPC_RMID, nullptr);
        XDestroyImage(img);
        m_shmImage = 0;
        delete info;
        m_shmInfo = 0;
        m_shmId = -1;
        m_shmData = nullptr;
        return false;
    }
    if (!XShmAttach(dpy, info)) {
        shmdt(info->shmaddr);
        shmctl(info->shmid, IPC_RMID, nullptr);
        XDestroyImage(img);
        m_shmImage = 0;
        delete info;
        m_shmInfo = 0;
        m_shmId = -1;
        m_shmData = nullptr;
        return false;
    }
    XSync(dpy, False);
    return true;
}

void ScreenSourceX11::destroyShm() {
    if (!m_display)
        return;
    if (m_shmInfo && m_shmImage) {
        XShmSegmentInfo *info = reinterpret_cast<XShmSegmentInfo *>(m_shmInfo);
        Display *dpy = static_cast<Display *>(m_display);
        XShmDetach(dpy, info);
        XDestroyImage(reinterpret_cast<XImage *>(m_shmImage));
        if (m_shmData && m_shmData != reinterpret_cast<void *>(-1))
            shmdt(m_shmData);
        if (m_shmId >= 0)
            shmctl(m_shmId, IPC_RMID, nullptr);
        delete info;
    }
    m_shmInfo = 0;
    m_shmImage = 0;
    m_shmData = nullptr;
    m_shmId = -1;
}

void ScreenSourceX11::shutdown() {
    destroyShm();
    if (m_display) {
        XCloseDisplay(static_cast<Display *>(m_display));
        m_display = nullptr;
    }
}

QImage ScreenSourceX11::grab() {
    if (!m_display)
        return QImage();
    Display *dpy = static_cast<Display *>(m_display);
    const int screen = DefaultScreen(dpy);
    const int w = DisplayWidth(dpy, screen);
    const int h = DisplayHeight(dpy, screen);
    if (w != m_width || h != m_height) { // resolution changed
        destroyShm();
        m_width = w;
        m_height = h;
        if (m_width <= 0 || m_height <= 0)
            return QImage();
        if (!initShm())
            return grabXGetImage();
    }

    if (m_shmInfo && m_shmImage) {
        XShmSegmentInfo *info = reinterpret_cast<XShmSegmentInfo *>(m_shmInfo);
        XImage *img = reinterpret_cast<XImage *>(m_shmImage);
        if (!XShmGetImage(dpy, DefaultRootWindow(dpy), img, 0, 0, AllPlanes)) {
            if (!XShmGetImage(dpy, DefaultRootWindow(dpy), img, 0, 0, AllPlanes))
                return QImage();
        }
        // ARGB32 on little-endian == BGRA byte order == X11 ZPixmap 32bpp
        QImage view(reinterpret_cast<const uchar *>(img->data), m_width, m_height,
                    img->bytes_per_line, QImage::Format_ARGB32);
        return view.copy();
    }
    return grabXGetImage();
}

QImage ScreenSourceX11::grabXGetImage() {
    Display *dpy = static_cast<Display *>(m_display);
    const int screen = DefaultScreen(dpy);
    XImage *img = XGetImage(dpy, DefaultRootWindow(dpy), 0, 0, m_width, m_height, AllPlanes, ZPixmap);
    if (!img)
        return QImage();
    QImage view(reinterpret_cast<const uchar *>(img->data), m_width, m_height,
                img->bytes_per_line, QImage::Format_ARGB32);
    QImage copy = view.copy();
    XDestroyImage(img);
    return copy;
}
