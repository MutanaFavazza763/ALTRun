#include "ui/WelcomeWindow.h"

#include <dwmapi.h>

#include <string>

#include "core/Localization.h"
#include "core/Logger.h"
#include "core/Text.h"
#include "ui/UiText.h"

namespace alt {
namespace {

constexpr wchar_t kWindowClass[] = L"ALTRun.WelcomeWindow.v2";

// Layout in 96-DPI design units (scaled through WelcomeWindow::S()).
constexpr int kWindowWidth = 470;
constexpr int kWindowHeight = 430;
constexpr int kHeaderHeight = 44;
constexpr int kMargin = 20;
constexpr int kContentWidth = kWindowWidth - 2 * kMargin;

constexpr int kIntroY = 54;
constexpr int kGuideY = 84;
constexpr int kGuideHeight = 96;
constexpr int kLanguageLabelY = 198;
constexpr int kLanguageComboY = 194;
constexpr int kAutoRunY = 236;
constexpr int kSendToY = 266;
constexpr int kNoteY = 302;
constexpr int kNoteHeight = 44;
constexpr int kButtonWidth = 150;
constexpr int kButtonHeight = 32;
constexpr int kButtonY = 378;

constexpr int kIdStart = IDOK;

const wchar_t* const kLanguageNames[3] = {L"简体中文", L"繁體中文", L"English"};
const char* const kLanguageValues[3] = {"zh_cn", "zh_hk", "en_us"};

// Replaces the first "%s" token (the guide uses it for the hotkey).
std::wstring ReplaceToken(std::wstring text, const std::wstring& value) {
    const std::wstring token = L"%s";
    const size_t position = text.find(token);
    if (position != std::wstring::npos) {
        text.replace(position, token.size(), value);
    }
    return text;
}

}  // namespace

WelcomeWindow::WelcomeWindow(Settings& settings) : settings_(settings) {}

WelcomeWindow::~WelcomeWindow() { DestroyResources(); }

bool WelcomeWindow::CreateWelcomeWindow(HINSTANCE instance) {
    if (!Create(instance, kWindowClass, Loc("resWelcomeTitle"),
                WS_POPUP | WS_CLIPCHILDREN,
                WS_EX_TOPMOST | WS_EX_TOOLWINDOW)) {
        return false;
    }
    const DWORD corner_preference = 2;  // DWMWCP_ROUND (Windows 11), best effort
    ::DwmSetWindowAttribute(hwnd_, 33, &corner_preference,
                            sizeof(corner_preference));
    return true;
}

int WelcomeWindow::S(int value) const {
    return ::MulDiv(value, dpi_, USER_DEFAULT_SCREEN_DPI);
}

void WelcomeWindow::CreateFonts() {
    NONCLIENTMETRICSW metrics = {};
    metrics.cbSize = static_cast<UINT>(sizeof(metrics));
    if (::SystemParametersInfoW(SPI_GETNONCLIENTMETRICS,
                                static_cast<UINT>(sizeof(metrics)), &metrics,
                                0) != FALSE) {
        LOGFONTW log_font = metrics.lfMessageFont;
        log_font.lfHeight = -::MulDiv(9, dpi_, 72);
        font_ = ::CreateFontIndirectW(&log_font);
    }
    if (font_ == nullptr) {
        font_ = static_cast<HFONT>(::GetStockObject(DEFAULT_GUI_FONT));
        font_is_stock_ = true;
    }

    LOGFONTW title_log = {};
    if (::GetObjectW(font_, static_cast<int>(sizeof(title_log)), &title_log) != 0) {
        title_log.lfHeight = static_cast<LONG>(title_log.lfHeight * 1.45);
        title_log.lfWeight = FW_SEMIBOLD;
        title_font_ = ::CreateFontIndirectW(&title_log);
    }
}

void WelcomeWindow::DestroyResources() {
    if (title_font_ != nullptr) {
        ::DeleteObject(title_font_);
        title_font_ = nullptr;
    }
    if (font_ != nullptr) {
        if (!font_is_stock_) {
            ::DeleteObject(font_);
        }
        font_ = nullptr;
    }
    if (background_brush_ != nullptr) {
        ::DeleteObject(background_brush_);
        background_brush_ = nullptr;
    }
}

void WelcomeWindow::ApplyFont(HWND control) const {
    if (font_ != nullptr && control != nullptr) {
        ::SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font_), TRUE);
    }
}

HWND WelcomeWindow::MakeControl(const wchar_t* class_name, const wchar_t* text,
                                DWORD style, int x, int y, int width, int height,
                                int id) {
    HWND control = ::CreateWindowExW(
        0, class_name, text, style, S(x), S(y), S(width), S(height), hwnd_,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), instance_, nullptr);
    if (control == nullptr) {
        Logger::Instance().Warn(std::string("welcome window: cannot create ") +
                                ToUtf8(class_name));
        return nullptr;
    }
    ApplyFont(control);
    return control;
}

void WelcomeWindow::CreateControls() {
    intro_text_ = MakeControl(WC_STATICW, L"", WS_CHILD | WS_VISIBLE | SS_LEFT,
                              kMargin, kIntroY, kContentWidth, 24, 0);
    guide_text_ = MakeControl(WC_STATICW, L"", WS_CHILD | WS_VISIBLE | SS_LEFT,
                              kMargin, kGuideY, kContentWidth, kGuideHeight, 0);

    language_label_ = MakeControl(WC_STATICW, L"", WS_CHILD | WS_VISIBLE | SS_LEFT,
                                   kMargin, kLanguageLabelY, 110, 24, 0);
    language_combo_ = MakeControl(
        WC_COMBOBOXW, L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
        kMargin + 120, kLanguageComboY, 190, 200, 0);
    if (language_combo_ != nullptr) {
        for (const wchar_t* name : kLanguageNames) {
            ::SendMessageW(language_combo_, CB_ADDSTRING, 0,
                           reinterpret_cast<LPARAM>(name));
        }
    }

    auto_run_check_ = MakeControl(WC_BUTTONW, L"",
                                  WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                                      BS_AUTOCHECKBOX,
                                  kMargin, kAutoRunY, kContentWidth, 24, 0);
    send_to_check_ = MakeControl(WC_BUTTONW, L"",
                                 WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                                     BS_AUTOCHECKBOX,
                                 kMargin, kSendToY, kContentWidth, 24, 0);
    note_text_ = MakeControl(WC_STATICW, L"", WS_CHILD | WS_VISIBLE | SS_LEFT,
                             kMargin, kNoteY, kContentWidth, kNoteHeight, 0);
    start_button_ = MakeControl(
        WC_BUTTONW, L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
        kWindowWidth - kMargin - kButtonWidth, kButtonY, kButtonWidth,
        kButtonHeight, kIdStart);
}

void WelcomeWindow::RefreshTexts() {
    if (hwnd_ == nullptr) {
        return;
    }
    const Localization& text = Localization::Instance();
    ::SetWindowTextW(hwnd_, text.Get("resWelcomeTitle").c_str());
    ::SetWindowTextW(intro_text_, text.Get("resWelcomeIntro").c_str());
    std::wstring hotkey = ToWide(settings_.hotkey1);
    if (hotkey.empty()) {
        hotkey = L"Alt + R";
    }
    ::SetWindowTextW(
        guide_text_,
        ReplaceToken(text.Get("resWelcomeGuide"), hotkey).c_str());
    ::SetWindowTextW(language_label_,
                     text.Get("resWelcomeLanguage").c_str());
    ::SetWindowTextW(auto_run_check_,
                     text.Get("resWelcomeAutoRun").c_str());
    ::SetWindowTextW(send_to_check_,
                     text.Get("resWelcomeSendTo").c_str());
    ::SetWindowTextW(note_text_, text.Get("resWelcomeNote").c_str());
    ::SetWindowTextW(start_button_, text.Get("resWelcomeStart").c_str());

    int index = 2;  // en_us
    if (settings_.lang == "zh_cn") {
        index = 0;
    } else if (settings_.lang == "zh_hk") {
        index = 1;
    }
    ::SendMessageW(language_combo_, CB_SETCURSEL, index, 0);
    ::SendMessageW(auto_run_check_, BM_SETCHECK,
                   settings_.auto_run ? BST_CHECKED : BST_UNCHECKED, 0);
    ::SendMessageW(send_to_check_, BM_SETCHECK,
                   settings_.add_to_send_to ? BST_CHECKED : BST_UNCHECKED, 0);
}

void WelcomeWindow::OnCreate() {
    dpi_ = static_cast<int>(::GetDpiForWindow(hwnd_));
    if (dpi_ <= 0) {
        dpi_ = USER_DEFAULT_SCREEN_DPI;
    }
    CreateFonts();
    background_brush_ = ::CreateSolidBrush(::GetSysColor(COLOR_BTNFACE));
    CreateControls();
    RefreshTexts();
    Logger::Instance().Trace("welcome window created");
}

void WelcomeWindow::Open() {
    if (hwnd_ == nullptr) {
        return;
    }
    RefreshTexts();
    PlaceOnCursorMonitor();
    ::ShowWindow(hwnd_, SW_SHOWNORMAL);
    ::SetForegroundWindow(hwnd_);
    if (start_button_ != nullptr) {
        ::SetFocus(start_button_);
    }
    Logger::Instance().Info("first run: welcome window opened");
}

void WelcomeWindow::PlaceOnCursorMonitor() {
    if (hwnd_ == nullptr) {
        return;
    }
    POINT cursor = {};
    ::GetCursorPos(&cursor);
    HMONITOR monitor = ::MonitorFromPoint(cursor, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info = {};
    info.cbSize = static_cast<DWORD>(sizeof(info));
    ::GetMonitorInfoW(monitor, &info);

    const int width = S(kWindowWidth);
    const int height = S(kWindowHeight);
    const int work_width = info.rcWork.right - info.rcWork.left;
    const int work_height = info.rcWork.bottom - info.rcWork.top;
    const int x = info.rcWork.left + (work_width - width) / 2;
    const int y = info.rcWork.top + (work_height - height) / 2;

    ::SetWindowPos(hwnd_, HWND_TOPMOST, x, y, width, height, SWP_NOACTIVATE);
}

void WelcomeWindow::DrawHeader(HDC dc) const {
    if (font_ == nullptr || title_font_ == nullptr) {
        return;
    }
    RECT client = {};
    ::GetClientRect(hwnd_, &client);

    ::SetBkMode(dc, TRANSPARENT);
    ::SetTextColor(dc, ::GetSysColor(COLOR_BTNTEXT));
    HGDIOBJ previous = ::SelectObject(dc, title_font_);
    RECT title = {S(20), S(8), client.right - S(20), S(kHeaderHeight - 6)};
    const std::wstring header = Loc("resWelcomeTitle");
    ::DrawTextW(dc, header.c_str(), -1, &title,
                DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
    ::SelectObject(dc, previous);

    const RECT separator = {S(16), S(kHeaderHeight), client.right - S(16),
                            S(kHeaderHeight) + 1};
    ::FillRect(dc, &separator, ::GetSysColorBrush(COLOR_3DSHADOW));
}

void WelcomeWindow::Accept() {
    int index = static_cast<int>(
        ::SendMessageW(language_combo_, CB_GETCURSEL, 0, 0));
    if (index < 0 || index > 2) {
        index = 2;
    }
    settings_.lang = kLanguageValues[index];
    settings_.auto_run =
        ::SendMessageW(auto_run_check_, BM_GETCHECK, 0, 0) == BST_CHECKED;
    settings_.add_to_send_to =
        ::SendMessageW(send_to_check_, BM_GETCHECK, 0, 0) == BST_CHECKED;
    settings_.first_run = false;
    settings_.Save();
    Logger::Instance().Info("first run: language=" + settings_.lang +
                            " AutoRun=" + (settings_.auto_run ? "1" : "0") +
                            " AddToSendTo=" +
                            (settings_.add_to_send_to ? "1" : "0"));
    Hide();
    if (accept_handler_) {
        accept_handler_();
    }
}

LRESULT WelcomeWindow::HandleMessage(UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
        case WM_COMMAND:
            if (HIWORD(wparam) == BN_CLICKED && LOWORD(wparam) == kIdStart) {
                Accept();
                return 0;
            }
            return 0;
        case WM_CLOSE:
            // There is no cancel: closing must still write the ini, otherwise
            // the welcome window would reappear on every launch.
            Accept();
            return 0;
        case WM_KEYDOWN:
            if (wparam == VK_ESCAPE) {
                Accept();
                return 0;
            }
            break;
        case WM_ERASEBKGND: {
            HDC dc = reinterpret_cast<HDC>(wparam);
            RECT client = {};
            ::GetClientRect(hwnd_, &client);
            if (background_brush_ != nullptr) {
                ::FillRect(dc, &client, background_brush_);
            }
            return 1;
        }
        case WM_PAINT: {
            PAINTSTRUCT paint = {};
            HDC dc = ::BeginPaint(hwnd_, &paint);
            DrawHeader(dc);
            ::EndPaint(hwnd_, &paint);
            return 0;
        }
        case WM_CTLCOLORSTATIC: {
            HDC dc = reinterpret_cast<HDC>(wparam);
            ::SetBkMode(dc, TRANSPARENT);
            return reinterpret_cast<LRESULT>(background_brush_);
        }
        case WM_DESTROY:
            DestroyResources();
            return 0;
        default:
            break;
    }
    return Window::HandleMessage(message, wparam, lparam);
}

}  // namespace alt
