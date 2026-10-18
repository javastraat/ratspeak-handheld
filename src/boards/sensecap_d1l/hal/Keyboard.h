#pragma once

#include "input/KeyEvent.h"

// This board has no physical keyboard — HAS_KEYBOARD is false and nothing
// should ever instantiate this class. It exists only because UIManager.h
// unconditionally includes "hal/Keyboard.h" (every board provides one),
// even though UIManager.h itself never references the type.
class Keyboard {
public:
    bool begin() { return false; }
    void update() {}
    void discardPending() {}
    InputMode getMode() const { return _mode; }
    void setMode(InputMode mode) { _mode = mode; }
    bool hasEvent() const { return false; }
    const KeyEvent& getEvent() const { return _event; }

private:
    InputMode _mode = InputMode::Navigation;
    KeyEvent _event;
};
