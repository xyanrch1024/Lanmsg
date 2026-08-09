#pragma once

#include "remote/IInputSink.h"

// Windows implementation using SetCursorPos (absolute mouse move, primary
// screen, physical pixels) and SendInput (mouse buttons / wheel / keyboard).
class InputSinkWindows : public IInputSink {
public:
    InputSinkWindows();
    ~InputSinkWindows() override;

    bool initialize() override;
    void shutdown() override;
    void mouseMove(int x, int y) override;
    void mouseButton(int button, bool down, int x, int y) override;
    void wheel(int dx, int dy) override;
    void key(int qtKey, bool down) override;

private:
    bool m_initialized = false;
};
