#include "app/App.h"

#include <windows.h>
#include <shellapi.h>
#include <strsafe.h>

#include <string>

#include "app/DesktopSpace.h"
#include "app/ShellIntegration.h"
#include "core/Localization.h"
#include "core/Logger.h"
#include "core/Text.h"

namespace alt {
namespace {

constexpr wchar_t kSingleInstanceMutex[] = L"ALTRun.SingleInstance.v2";
constexpr wchar_t kMainWindowClass[] = L"ALTRun.MainWindow.v2";

constexpr UINT kHotkeyIdToggle = 1;
constexpr UINT kHotkeyIdToggle2 = 3;
constexpr UINT kHotkeyIdLastItem = 2;

constexpr UINT kTrayCallbackMessage = WM_APP + 10;
constexpr UINT kShowMessage = WM_APP + 11;

constexpr UINT kTrayMenuShow = 100;
constexpr UINT kTrayMenuConfig = 101;
constexpr UINT kTrayMenuExit = 102;
constexpr UINT kTrayMenuManager = 103;
constexpr UINT kTrayMenuAbout = 104;

// "Alt + R" style text is stored in the settings; this is the fallback used
// when neither the ini nor the legacy defaults produce a valid hotkey.
constexpr wchar_t kAutoRunKey[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr wchar_t kAutoRunValue[] = L"ALTRun";

std::wstring ExePath() {
    wchar_t buffer[MAX_PATH * 2] = {};
    ::GetModuleFileNameW(nullptr, buffer, static_cast<DWORD>(std::size(buffer)));
    return buffer;
}

// Development convenience: `ALTRun.exe --show` opens the launcher immediately
// and `--config` opens the settings window, so the UI can be inspected without
// pressing the hotkey or clicking the tray icon.
bool HasFlag(const wchar_t* flag) {
    if (flag == nullptr) {
        return false;
    }
    int argc = 0;
    LPWSTR* argv = ::CommandLineToArgvW(::GetCommandLineW(), &argc);
    if (argv == nullptr) {
        return false;
    }
    bool found = false;
    for (int i = 1; i < argc && !found; ++i) {
        found = ::_wcsicmp(argv[i], flag) == 0;
    }
    ::LocalFree(argv);
    return found;
}

}  // namespace

App::App(HINSTANCE instance)
    : instance_(instance),
      main_window_(settings_, store_, param_history_),
      config_window_(settings_),
      manager_window_(settings_, store_),
      about_window_(settings_) {}

App::~App() {
    RemoveTrayIcon();
    hotkey_.Unregister();
    last_item_hotkey_.Unregister();
    ReleaseSingleInstance();
}

int App::Run() {
    if (!AcquireSingleInstance()) {
        NotifyExistingInstance();
        return 0;
    }

    settings_.Load();
    Logger::Instance().SetEnabled(settings_.trace_enabled);
    Logger::Instance().Info("ALTRun starting, exe=" + ToUtf8(ExePath()));

    // Built-in language resources must be ready before any window is shown;
    // changing Lang later re-runs this in ApplySettings().
    Localization::Instance().Load(settings_.lang);
    // AddToSendTo is a system integration that is re-applied on every startup
    // (idempotent), exactly like the original.
    ShellIntegration::ApplyAddToSendTo(settings_.add_to_send_to, ExePath());

    // Load the portable shortcut list; a missing ShortCutList.txt gets the
    // built-in default list, exactly like the original (deleting the file
    // regenerates it).
    store_.Load();
    store_.SeedDefaultsIfMissing();
    // Recent list and parameter history are stored outside ShortCutList.txt
    // (ini LatestList / ParamHistory.txt), like the original.
    store_.SetLatestFromSpec(settings_.latest_list);
    param_history_.Load(settings_.param_history_limit);

    if (!main_window_.CreateMain(instance_)) {
        Logger::Instance().Error("cannot create main window");
        return 1;
    }
    main_window_.SetExitHandler([this]() { RequestExit(); });
    main_window_.SetExternalHandler(
        [this](HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
            return HandleExternalMessage(hwnd, message, wparam, lparam);
        });

    toggle_spec_ = ParseHotkey(settings_.hotkey1);
    // Secondary toggle hotkey (HotKey2); empty or identical values disable it.
    if (!settings_.hotkey2.empty() && settings_.hotkey2 != settings_.hotkey1) {
        secondary_spec_ = ParseHotkey(settings_.hotkey2);
    }
    last_item_spec_ = ParseHotkey(settings_.last_item_hotkey);
    RegisterHotkeys();

    // Settings dialog: the tray menu opens it; OK re-applies hotkeys and the
    // launcher window's appearance.
    if (config_window_.CreateConfigWindow(instance_)) {
        config_window_.SetApplyHandler([this]() { ApplySettings(); });
    } else {
        Logger::Instance().Warn("cannot create settings window");
    }

    // Shortcut manager: edits are saved immediately, so the handler only has to
    // refresh the launcher's cached match list.
    if (manager_window_.CreateManagerWindow(instance_)) {
        manager_window_.SetChangedHandler([this]() {
            // Row indices changed, so previously recorded latest-list indices
            // no longer point at the same items.
            store_.SetLatestFromSpec("");
            main_window_.RefreshMatches();
        });
    } else {
        Logger::Instance().Warn("cannot create shortcut manager window");
    }

    // About window (tray menu "关于"): native controls, created once and reused.
    if (!about_window_.CreateAboutWindow(instance_)) {
        Logger::Instance().Warn("cannot create about window");
    }

    if (settings_.show_tray_icon) {
        AddTrayIcon();
    }
    ApplyAutoRun(settings_.auto_run);

    if (HasFlag(L"--config") && config_window_.hwnd() != nullptr) {
        config_window_.Open();
    }
    if (HasFlag(L"--manager") && manager_window_.hwnd() != nullptr) {
        manager_window_.Open();
    }
    if (HasFlag(L"--about") && about_window_.hwnd() != nullptr) {
        about_window_.Open();
    }

    if (settings_.show_me_when_start || HasFlag(L"--show")) {
        main_window_.ShowLauncher();
    }

    MSG message = {};
    while (::GetMessageW(&message, nullptr, 0, 0) > 0) {
        ::TranslateMessage(&message);
        ::DispatchMessageW(&message);
    }

    Logger::Instance().Info("ALTRun shutting down");
    settings_.latest_list = store_.LatestIndexSpec();
    settings_.Save();
    store_.Save();
    param_history_.Save(settings_.param_history_limit);
    return static_cast<int>(message.wParam);
}

void App::RequestExit() {
    if (quitting_) {
        return;
    }
    quitting_ = true;
    ::PostQuitMessage(0);
}

bool App::AcquireSingleInstance() {
    instance_mutex_ = ::CreateMutexW(nullptr, FALSE, kSingleInstanceMutex);
    if (instance_mutex_ == nullptr) {
        return true;  // fail open: still usable
    }
    if (::GetLastError() == ERROR_ALREADY_EXISTS) {
        ReleaseSingleInstance();
        return false;
    }
    return true;
}

void App::ReleaseSingleInstance() {
    if (instance_mutex_ != nullptr) {
        ::CloseHandle(instance_mutex_);
        instance_mutex_ = nullptr;
    }
}

void App::NotifyExistingInstance() {
    HWND existing = ::FindWindowW(kMainWindowClass, nullptr);
    if (existing != nullptr) {
        ::PostMessageW(existing, kShowMessage, 0, 0);
    }
}

LRESULT App::HandleExternalMessage(HWND hwnd, UINT message, WPARAM wparam,
                                   LPARAM lparam) {
    switch (message) {
        case WM_HOTKEY:
            if (wparam == kHotkeyIdToggle || wparam == kHotkeyIdToggle2) {
                EnsureLauncherOnCurrentDesktop();
                main_window_.ToggleLauncher();
                return 0;
            }
            if (wparam == kHotkeyIdLastItem) {
                EnsureLauncherOnCurrentDesktop();
                main_window_.LaunchLatest();
                return 0;
            }
            return 0;
        case kShowMessage:
            EnsureLauncherOnCurrentDesktop();
            main_window_.ShowLauncher();
            return 0;
        case kTrayCallbackMessage:
            OnTrayCallback(lparam);
            return 0;
        case WM_COMMAND:
            switch (LOWORD(wparam)) {
                case kTrayMenuShow:
                    EnsureLauncherOnCurrentDesktop();
                    main_window_.ShowLauncher();
                    return 0;
                case kTrayMenuManager:
                    if (manager_window_.hwnd() != nullptr) {
                        manager_window_.Open();
                    }
                    return 0;
                case kTrayMenuConfig:
                    if (config_window_.hwnd() != nullptr) {
                        config_window_.Open();
                    }
                    return 0;
                case kTrayMenuAbout:
                    if (about_window_.hwnd() != nullptr) {
                        about_window_.Open();
                    }
                    return 0;
                case kTrayMenuExit:
                    RequestExit();
                    return 0;
                default:
                    break;
            }
            return 0;
        case WM_CLOSE:
            // Closing the launcher window terminates the process (original
            // actClose behaviour): flush ini, shortcut list and param history.
            RequestExit();
            return 0;
        case WM_ENDSESSION:
            RequestExit();
            return 0;
        default:
            break;
    }
    return ::DefWindowProcW(hwnd, message, wparam, lparam);
}

void App::OnTrayCallback(LPARAM lparam) {
    switch (lparam) {
        case WM_LBUTTONUP:
        case WM_LBUTTONDBLCLK:
            EnsureLauncherOnCurrentDesktop();
            main_window_.ToggleLauncher();
            break;
        case WM_RBUTTONUP:
            ShowTrayMenu();
            break;
        default:
            break;
    }
}

void App::ShowTrayMenu() {
    // app must not depend on ui/UiText.h (layering is ui -> app -> core), so the
    // lookup goes straight through Localization here.
    const Localization& text = Localization::Instance();
    HMENU menu = ::CreatePopupMenu();
    ::AppendMenuW(menu, MF_STRING, kTrayMenuShow,
                  text.Get("resMenuShow").c_str());
    ::AppendMenuW(menu, MF_STRING, kTrayMenuManager,
                  text.Get("resMenuManager").c_str());
    ::AppendMenuW(menu, MF_STRING, kTrayMenuConfig,
                  text.Get("resMenuConfig").c_str());
    ::AppendMenuW(menu, MF_STRING, kTrayMenuAbout,
                  text.Get("Common.MenuAbout").c_str());
    ::AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    ::AppendMenuW(menu, MF_STRING, kTrayMenuExit,
                  text.Get("Common.MenuClose").c_str());

    POINT cursor = {};
    ::GetCursorPos(&cursor);
    ::SetForegroundWindow(main_window_.hwnd());
    ::TrackPopupMenu(menu, TPM_LEFTBUTTON | TPM_RIGHTBUTTON, cursor.x, cursor.y, 0,
                     main_window_.hwnd(), nullptr);
    ::DestroyMenu(menu);
}

void App::AddTrayIcon() {
    HWND hwnd = main_window_.hwnd();
    if (hwnd == nullptr) {
        return;
    }
    tray_icon_ = {};
    tray_icon_.cbSize = sizeof(tray_icon_);
    tray_icon_.hWnd = hwnd;
    tray_icon_.uID = 1;
    tray_icon_.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    tray_icon_.uCallbackMessage = kTrayCallbackMessage;
    tray_icon_.hIcon = ::LoadIconW(nullptr, IDI_APPLICATION);
    ::StringCchCopyW(tray_icon_.szTip, std::size(tray_icon_.szTip),
                     Localization::Instance().Get("resTrayTip").c_str());
    tray_visible_ = ::Shell_NotifyIconW(NIM_ADD, &tray_icon_) != FALSE;
    if (!tray_visible_) {
        Logger::Instance().Warn("failed to add tray icon, error=" +
                                std::to_string(::GetLastError()));
    }
}

void App::RemoveTrayIcon() {
    if (tray_visible_) {
        ::Shell_NotifyIconW(NIM_DELETE, &tray_icon_);
        tray_visible_ = false;
    }
}

void App::ApplySettings() {
    // Hotkeys: re-register from the (already saved) settings.
    UnregisterHotkeys();
    toggle_spec_ = ParseHotkey(settings_.hotkey1);
    secondary_spec_ = {};
    if (!settings_.hotkey2.empty() && settings_.hotkey2 != settings_.hotkey1) {
        secondary_spec_ = ParseHotkey(settings_.hotkey2);
    }
    last_item_spec_ = ParseHotkey(settings_.last_item_hotkey);
    RegisterHotkeys();

    Logger::Instance().SetEnabled(settings_.trace_enabled);
    ApplyAutoRun(settings_.auto_run);
    // Language may have changed (ConfigWindow writes [GUI] Lang): select the
    // new built-in locale before the launcher re-renders.
    Localization::Instance().Load(settings_.lang);
    // AddToSendTo must take effect immediately when the checkbox is toggled.
    ShellIntegration::ApplyAddToSendTo(settings_.add_to_send_to, ExePath());
    main_window_.ApplySettings();
    if (about_window_.hwnd() != nullptr) {
        about_window_.RefreshTexts();
    }
    Logger::Instance().Info("settings applied");
}

void App::RegisterHotkeys() {
    if (main_window_.hwnd() == nullptr) {
        return;
    }
    if (!hotkey_.Register(main_window_.hwnd(), kHotkeyIdToggle, toggle_spec_)) {
        Logger::Instance().Warn("cannot register hotkey " + settings_.hotkey1);
    }
    if (secondary_spec_.valid() &&
        !secondary_hotkey_.Register(main_window_.hwnd(), kHotkeyIdToggle2,
                                    secondary_spec_)) {
        Logger::Instance().Warn("cannot register hotkey " + settings_.hotkey2);
    }
    if (last_item_spec_.valid() &&
        !last_item_hotkey_.Register(main_window_.hwnd(), kHotkeyIdLastItem,
                                    last_item_spec_)) {
        Logger::Instance().Warn("cannot register hotkey " + settings_.last_item_hotkey);
    }
}

void App::UnregisterHotkeys() {
    hotkey_.Unregister();
    secondary_hotkey_.Unregister();
    last_item_hotkey_.Unregister();
}

bool App::RecreateMainWindow() {
    // Everything Windows anchors to a window handle has to move to the new one:
    // the tray icon callback and all three RegisterHotKey registrations. Tray
    // icon removal therefore happens before the old window disappears.
    UnregisterHotkeys();
    RemoveTrayIcon();

    main_window_.Destroy();
    if (!main_window_.CreateMain(instance_)) {
        Logger::Instance().Error("cannot recreate main window");
        return false;
    }
    main_window_.SetExitHandler([this]() { RequestExit(); });
    main_window_.SetExternalHandler(
        [this](HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
            return HandleExternalMessage(hwnd, message, wparam, lparam);
        });
    RegisterHotkeys();
    if (settings_.show_tray_icon) {
        AddTrayIcon();
    }
    return true;
}

void App::EnsureLauncherOnCurrentDesktop() {
    if (main_window_.hwnd() == nullptr) {
        return;
    }
    if (IsWindowOnCurrentDesktop(main_window_.hwnd())) {
        return;
    }
    Logger::Instance().Info("launcher is on another virtual desktop, recreating it");
    RecreateMainWindow();
}

void App::ApplyAutoRun(bool enabled) {
    HKEY key = nullptr;
    if (::RegOpenKeyExW(HKEY_CURRENT_USER, kAutoRunKey, 0, KEY_SET_VALUE, &key) !=
        ERROR_SUCCESS) {
        return;
    }
    if (enabled) {
        const std::wstring quoted = L"\"" + ExePath() + L"\"";
        const auto bytes = static_cast<DWORD>((quoted.size() + 1) * sizeof(wchar_t));
        ::RegSetValueExW(key, kAutoRunValue, 0, REG_SZ,
                         reinterpret_cast<const BYTE*>(quoted.c_str()), bytes);
    } else {
        ::RegDeleteValueW(key, kAutoRunValue);
    }
    ::RegCloseKey(key);
}

}  // namespace alt
