#pragma once

#include "remote/IInputSink.h"

// X11 implementation using XTest fake events.
class InputSinkX11 : public IInputSink {
public:
    InputSinkX11();
    ~InputSinkX11() override;

    bool initialize() override;
    void shutdown() override;
    void mouseMove(int x, int y) override;
    void mouseButton(int button, bool down, int x, int y) override;
    void wheel(int dx, int dy) override;
    void key(int qtKey, bool down) override;

private:
    void *m_display = nullptr; // Display*
    int m_screen = 0;
};
