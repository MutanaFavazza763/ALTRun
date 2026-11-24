#pragma once

#include <windows.h>

namespace alt {

// Reports whether |hwnd| lives on the virtual desktop ("Task View" space) that
// the user is looking at right now.
//
// Windows never moves a top-level window between virtual desktops. A launcher
// created on desktop 1 therefore stays on desktop 1 when the user switches to
// desktop 2 and presses the global hotkey: Win32 still calls it visible, yet
// it is nowhere on screen and the hotkey appears dead.
// IVirtualDesktopManager (Windows 10+) can only report that condition, so the
// remedy is to create a fresh top-level window, which always belongs to the
// desktop that is active at creation time.
//
// Returns true when the state cannot be determined (COM missing, shell without
// virtual desktops), so callers keep their normal path instead of rebuilding
// windows on every show.
bool IsWindowOnCurrentDesktop(HWND hwnd);

}  // namespace alt
