#include "remote/ScreenSourceWindows.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <QDebug>

ScreenSourceWindows::ScreenSourceWindows() = default;

ScreenSourceWindows::~ScreenSourceWindows() {
    shutdown();
}

bool ScreenSourceWindows::initialize() {
    if (m_screenDc)
        return true;

    const int w = GetSystemMetrics(SM_CXSCREEN);
    const int h = GetSystemMetrics(SM_CYSCREEN);
    if (w <= 0 || h <= 0) {
        qWarning() << "ScreenSourceWindows: invalid screen size" << w << h;
        return false;
    }

    HDC screenDc = GetDC(nullptr);
    if (!screenDc) {
        qWarning() << "ScreenSourceWindows: cannot get screen DC";
        return false;
    }

    HDC memDc = CreateCompatibleDC(screenDc);
    if (!memDc) {
        qWarning() << "ScreenSourceWindows: cannot create memory DC";
        ReleaseDC(nullptr, screenDc);
        return false;
    }

    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = -h; // top-down rows
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    void *bits = nullptr;
    HBITMAP bmp = CreateDIBSection(screenDc, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!bmp || !bits) {
        qWarning() << "ScreenSourceWindows: CreateDIBSection failed";
        if (bmp)
            DeleteObject(bmp);
        DeleteDC(memDc);
        ReleaseDC(nullptr, screenDc);
        return false;
    }
    if (SelectObject(memDc, bmp) == HGDI_ERROR) {
        DeleteObject(bmp);
        DeleteDC(memDc);
        ReleaseDC(nullptr, screenDc);
        return false;
    }

    m_screenDc = screenDc;
    m_memDc = memDc;
    m_bitmap = bmp;
    m_bits = bits;
    m_width = w;
    m_height = h;
    return true;
}

void ScreenSourceWindows::shutdown() {
    if (m_bitmap) {
        DeleteObject(static_cast<HBITMAP>(m_bitmap));
        m_bitmap = nullptr;
    }
    if (m_memDc) {
        DeleteDC(static_cast<HDC>(m_memDc));
        m_memDc = nullptr;
    }
    if (m_screenDc) {
        ReleaseDC(nullptr, static_cast<HDC>(m_screenDc));
        m_screenDc = nullptr;
    }
    m_bits = nullptr;
    m_width = 0;
    m_height = 0;
}

QImage ScreenSourceWindows::grab() {
    if (!m_screenDc || !m_bits)
        return {};
    if (!BitBlt(static_cast<HDC>(m_memDc), 0, 0, m_width, m_height,
                static_cast<HDC>(m_screenDc), 0, 0, SRCCOPY))
        return {};
    // DIB 32bpp layout is 0x00RRGGBB (Format_RGB32); convert to match the
    // documented ARGB32 contract used by the capture worker.
    QImage view(static_cast<uchar *>(m_bits), m_width, m_height, m_width * 4, QImage::Format_RGB32);
    return view.convertToFormat(QImage::Format_ARGB32);
}
