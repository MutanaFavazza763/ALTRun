#include "app/App.h"

#include <windows.h>
#include <shellapi.h>
#include <strsafe.h>

#include <string>

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
// so the UI can be inspected without pressing the hotkey.
bool HasShowFlag() {
    int argc = 0;
    LPWSTR* argv = ::CommandLineToArgvW(::GetCommandLineW(), &argc);
    if (argv == nullptr) {
        return false;
    }
    bool found = false;
    for (int i = 1; i < argc && !found; ++i) {
        found = ::_wcsicmp(argv[i], L"--show") == 0;
    }
    ::LocalFree(argv);
    return found;
}

}  // namespace

App::App(HINSTANCE instance)
    : instance_(instance),
      main_window_(settings_, store_, param_history_) {}

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

    const HotkeySpec toggle = ParseHotkey(settings_.hotkey1);
    if (!hotkey_.Register(main_window_.hwnd(), kHotkeyIdToggle, toggle)) {
        Logger::Instance().Warn("cannot register hotkey " + settings_.hotkey1);
    }
    // Secondary toggle hotkey (HotKey2); empty or identical values disable it.
    if (!settings_.hotkey2.empty() && settings_.hotkey2 != settings_.hotkey1) {
        const HotkeySpec secondary = ParseHotkey(settings_.hotkey2);
        if (secondary.valid()) {
            secondary_hotkey_.Register(main_window_.hwnd(), kHotkeyIdToggle2, secondary);
        }
    }
    const HotkeySpec last_item = ParseHotkey(settings_.last_item_hotkey);
    if (last_item.valid()) {
        last_item_hotkey_.Register(main_window_.hwnd(), kHotkeyIdLastItem, last_item);
    }

    if (settings_.show_tray_icon) {
        AddTrayIcon();
    }
    ApplyAutoRun(settings_.auto_run);

    if (settings_.show_me_when_start || HasShowFlag()) {
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
                main_window_.ToggleLauncher();
                return 0;
            }
            if (wparam == kHotkeyIdLastItem) {
                main_window_.LaunchLatest();
                return 0;
            }
            return 0;
        case kShowMessage:
            main_window_.ShowLauncher();
            return 0;
        case kTrayCallbackMessage:
            OnTrayCallback(lparam);
            return 0;
        case WM_COMMAND:
            switch (LOWORD(wparam)) {
                case kTrayMenuShow:
                    main_window_.ShowLauncher();
                    return 0;
                case kTrayMenuConfig:
                    ::MessageBoxW(hwnd,
                                  L"设置窗口尚未实现（见 docs/ROADMAP.md 里程碑 M3）",
                                  L"ALTRun", MB_OK | MB_ICONINFORMATION);
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
    HMENU menu = ::CreatePopupMenu();
    ::AppendMenuW(menu, MF_STRING, kTrayMenuShow, L"打开 ALTRun(&O)");
    ::AppendMenuW(menu, MF_STRING, kTrayMenuConfig, L"设置(&S)");
    ::AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    ::AppendMenuW(menu, MF_STRING, kTrayMenuExit, L"退出(&X)");

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
                      L"ALTRun - Alt+R 打开");
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
