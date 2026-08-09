#pragma once

// Platform-agnostic remote input injection (mouse / wheel / keyboard).
class IInputSink {
public:
    virtual ~IInputSink() = default;
    virtual bool initialize() = 0;
    virtual void shutdown() = 0;
    virtual void mouseMove(int x, int y) = 0;
    virtual void mouseButton(int button, bool down, int x, int y) = 0;
    virtual void wheel(int dx, int dy) = 0;
    virtual void key(int qtKey, bool down) = 0;
};
