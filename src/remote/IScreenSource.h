#pragma once

#include <QImage>

// Platform-agnostic full-screen capture source (runs in its own thread).
class IScreenSource {
public:
    virtual ~IScreenSource() = default;
    // Open the display / capture handle. Must be called in the thread that will grab.
    virtual bool initialize() = 0;
    virtual void shutdown() = 0;
    // Grab current screen as ARGB32. Returns null image on failure.
    virtual QImage grab() = 0;
    virtual int width() const = 0;
    virtual int height() const = 0;
};
