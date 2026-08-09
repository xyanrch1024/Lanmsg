#pragma once

#include "remote/IScreenSource.h"

#include <QImage>

// Windows GDI implementation: BitBlt from the primary screen DC into a
// top-down DIB section. Physical pixels (DPI-consistent with the input
// injection done by InputSinkWindows via SetCursorPos).
class ScreenSourceWindows : public IScreenSource {
public:
    ScreenSourceWindows();
    ~ScreenSourceWindows() override;

    bool initialize() override;
    void shutdown() override;
    QImage grab() override;
    int width() const override { return m_width; }
    int height() const override { return m_height; }

private:
    void *m_screenDc = nullptr; // HDC of the primary screen
    void *m_memDc = nullptr;    // memory DC (compatible)
    void *m_bitmap = nullptr;   // DIB section HBITMAP
    void *m_bits = nullptr;     // DIB pixel buffer (owned by m_bitmap)
    int m_width = 0;
    int m_height = 0;
};
