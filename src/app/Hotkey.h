#pragma once

#include <windows.h>

#include <string>

namespace alt {

// A parsed global hotkey, e.g. "Alt + R".
struct HotkeySpec {
    unsigned mods = 0;   // MOD_ALT | MOD_CONTROL | MOD_SHIFT | MOD_WIN
    unsigned vk = 0;     // virtual key code
    std::string text;    // original text, kept for display

    bool valid() const { return vk != 0; }
};

// Parses legacy "Alt + R" / "Ctrl+Shift+F1" / "Pause" notation.
HotkeySpec ParseHotkey(const std::string& text);

// Registers/unregisters the hotkey for the given window.
class HotkeyRegistration {
public:
    HotkeyRegistration() = default;
    ~HotkeyRegistration();

    HotkeyRegistration(const HotkeyRegistration&) = delete;
    HotkeyRegistration& operator=(const HotkeyRegistration&) = delete;

    bool Register(HWND hwnd, int id, const HotkeySpec& spec);
    void Unregister();

    const HotkeySpec& spec() const { return spec_; }
    bool registered() const { return registered_; }

private:
    HWND hwnd_ = nullptr;
    int id_ = 0;
    HotkeySpec spec_;
    bool registered_ = false;
};

}  // namespace alt
