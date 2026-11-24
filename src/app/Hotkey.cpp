#include "app/Hotkey.h"

#include <windows.h>

#include <cctype>
#include <sstream>
#include <vector>

#include "core/Text.h"

namespace alt {
namespace {

std::string Normalize(std::string_view text) {
    std::string out;
    out.reserve(text.size());
    bool last_space = false;
    for (char c : text) {
        const char lower = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (lower == ' ' || lower == '\t') {
            if (!last_space && !out.empty()) {
                out.push_back(' ');
            }
            last_space = true;
            continue;
        }
        last_space = false;
        out.push_back(lower);
    }
    return out;
}

std::vector<std::string> SplitPlus(std::string_view text) {
    std::vector<std::string> parts;
    size_t start = 0;
    while (start <= text.size()) {
        const size_t plus = text.find('+', start);
        std::string_view piece = text.substr(
            start, plus == std::string_view::npos ? std::string_view::npos : plus - start);
        std::string normalized = Normalize(TrimAscii(piece));
        if (!normalized.empty()) {
            parts.push_back(std::move(normalized));
        }
        if (plus == std::string_view::npos) {
            break;
        }
        start = plus + 1;
    }
    return parts;
}

unsigned KeyForToken(const std::string& token) {
    if (token.size() == 1) {
        const char c = token[0];
        if (c >= 'a' && c <= 'z') {
            return static_cast<unsigned>(std::toupper(static_cast<unsigned char>(c)));
        }
        if (c >= '0' && c <= '9') {
            return static_cast<unsigned>(c);
        }
        if (c == '`') return VK_OEM_3;
    }
    if (token.size() >= 2 && token[0] == 'f') {
        const int number = std::atoi(token.c_str() + 1);
        if (number >= 1 && number <= 24) {
            return static_cast<unsigned>(VK_F1 + number - 1);
        }
    }
    if (token == "pause") return VK_PAUSE;
    if (token == "space") return VK_SPACE;
    if (token == "enter" || token == "return") return VK_RETURN;
    if (token == "tab") return VK_TAB;
    if (token == "insert" || token == "ins") return VK_INSERT;
    if (token == "delete" || token == "del") return VK_DELETE;
    if (token == "home") return VK_HOME;
    if (token == "end") return VK_END;
    if (token == "pageup" || token == "pgup") return VK_PRIOR;
    if (token == "pagedown" || token == "pgdn") return VK_NEXT;
    if (token == "left") return VK_LEFT;
    if (token == "right") return VK_RIGHT;
    if (token == "up") return VK_UP;
    if (token == "down") return VK_DOWN;
    if (token == "printscreen" || token == "prtsc") return VK_SNAPSHOT;
    if (token == "scrolllock") return VK_SCROLL;
    if (token == "numlock") return VK_NUMLOCK;
    if (token == "capslock") return VK_CAPITAL;
    return 0;
}

}  // namespace

HotkeySpec ParseHotkey(const std::string& text) {
    HotkeySpec spec;
    spec.text = text;

    const std::vector<std::string> parts = SplitPlus(text);
    if (parts.empty()) {
        return spec;
    }

    for (const std::string& part : parts) {
        if (part == "alt") {
            spec.mods |= MOD_ALT;
        } else if (part == "ctrl" || part == "control") {
            spec.mods |= MOD_CONTROL;
        } else if (part == "shift") {
            spec.mods |= MOD_SHIFT;
        } else if (part == "win" || part == "windows") {
            spec.mods |= MOD_WIN;
        } else {
            const unsigned vk = KeyForToken(part);
            if (vk != 0) {
                spec.vk = vk;
            }
        }
    }
    return spec;
}

HotkeyRegistration::~HotkeyRegistration() { Unregister(); }

bool HotkeyRegistration::Register(HWND hwnd, int id, const HotkeySpec& spec) {
    Unregister();
    if (!spec.valid() || hwnd == nullptr) {
        spec_ = spec;
        return false;
    }
    if (!::RegisterHotKey(hwnd, id, spec.mods | MOD_NOREPEAT, spec.vk)) {
        return false;
    }
    hwnd_ = hwnd;
    id_ = id;
    spec_ = spec;
    registered_ = true;
    return true;
}

void HotkeyRegistration::Unregister() {
    if (registered_ && hwnd_ != nullptr) {
        ::UnregisterHotKey(hwnd_, id_);
    }
    registered_ = false;
    hwnd_ = nullptr;
    id_ = 0;
}

}  // namespace alt
