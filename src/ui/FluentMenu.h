#pragma once

#include <windows.h>

#include <string>
#include <vector>

namespace alt {
namespace fluent {

// One row in the custom tray context menu.  Separator rows ignore every other
// field, and danger rows use the red hover state (the Close command).
struct MenuItem {
    int id = 0;
    std::wstring text;
    std::wstring glyph;
    bool separator = false;
    bool danger = false;
    bool enabled = true;
};

// Shows a borderless, WinUI-style popup menu at screen_point and returns the
// command id of the chosen row, or 0 when the user dismissed it.  The function
// runs a short modal message loop; the owner window keeps receiving its normal
// messages in the meantime.
int ShowPopupMenu(HWND owner, HINSTANCE instance, POINT screen_point,
                  const std::wstring& title, const std::wstring& subtitle,
                  HICON icon, const std::vector<MenuItem>& items);

}  // namespace fluent
}  // namespace alt
