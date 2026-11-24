#pragma once

#include <windows.h>
#include <shellapi.h>

#include <string>

#include "app/Hotkey.h"
#include "app/Settings.h"
#include "core/ParamHistory.h"
#include "core/ShortcutStore.h"
#include "ui/AboutWindow.h"
#include "ui/ConfigWindow.h"
#include "ui/MainWindow.h"
#include "ui/ShortcutManagerWindow.h"

namespace alt {

// Process wide orchestration: single instance guard, hotkey, tray icon,
// message loop and the lifetime of the launcher window.
class App {
public:
    explicit App(HINSTANCE instance);
    ~App();

    App(const App&) = delete;
    App& operator=(const App&) = delete;

    int Run();

    Settings& settings() { return settings_; }
    ShortcutStore& store() { return store_; }
    MainWindow& main_window() { return main_window_; }

    void RequestExit();

private:
    bool AcquireSingleInstance();
    void ReleaseSingleInstance();
    void NotifyExistingInstance();

    void AddTrayIcon();
    void RemoveTrayIcon();
    // Registers/unregisters all three global hotkeys against the launcher
    // window. They are tied to a window handle, so a rebuilt window needs them
    // moved to the new one.
    void RegisterHotkeys();
    void UnregisterHotkeys();
    // Windows cannot move windows between virtual desktops; when the launcher
    // lives on a desktop the user is not looking at, replace it with a fresh
    // window so the hotkey works from every virtual desktop.
    void EnsureLauncherOnCurrentDesktop();
    bool RecreateMainWindow();
    void ShowTrayMenu();
    void OnTrayCallback(LPARAM lparam);
    void ApplyAutoRun(bool enabled);
    // Re-applies settings that need runtime action: hotkeys, auto-run, logging
    // and the launcher window's alpha/size/timers.
    void ApplySettings();

    LRESULT HandleExternalMessage(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);

    HINSTANCE instance_ = nullptr;
    HANDLE instance_mutex_ = nullptr;

    Settings settings_;
    ShortcutStore store_;
    ParamHistory param_history_;
    MainWindow main_window_;
    ConfigWindow config_window_;
    ShortcutManagerWindow manager_window_;
    AboutWindow about_window_;
    HotkeyRegistration hotkey_;
    HotkeyRegistration secondary_hotkey_;
    HotkeyRegistration last_item_hotkey_;
    // Parsed once from Settings and reused whenever hotkeys are re-registered
    // (settings change or window rebuild).
    HotkeySpec toggle_spec_;
    HotkeySpec secondary_spec_;
    HotkeySpec last_item_spec_;

    NOTIFYICONDATAW tray_icon_ = {};
    bool tray_visible_ = false;
    bool quitting_ = false;
};

}  // namespace alt
