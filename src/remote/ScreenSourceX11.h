#pragma once

#include "remote/IScreenSource.h"

#include <QImage>

// X11 implementation using XShm (fallback to XGetImage).
class ScreenSourceX11 : public IScreenSource {
public:
    ScreenSourceX11();
    ~ScreenSourceX11() override;

    bool initialize() override;
    void shutdown() override;
    QImage grab() override;
    int width() const override { return m_width; }
    int height() const override { return m_height; }

private:
    bool initShm();
    void destroyShm();
    QImage grabXGetImage();

    void *m_display = nullptr; // Display*
    long m_shmInfo = 0;        // XShmSegmentInfo*
    long m_shmImage = 0;       // XImage*
    void *m_shmData = nullptr; // shmat result
    int m_shmId = -1;
    int m_width = 0;
    int m_height = 0;
};
