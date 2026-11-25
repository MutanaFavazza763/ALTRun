#include "ui/AboutWindow.h"

#include <commctrl.h>
#include <dwmapi.h>
#include <windowsx.h>

#include <string>

#include "app/ShellIntegration.h"
#include "core/Localization.h"
#include "core/Logger.h"
#include "core/Text.h"
#include "ui/FluentTheme.h"
#include "ui/UiText.h"

namespace alt {
namespace {

constexpr wchar_t kWindowClass[] = L"ALTRun.AboutWindow.v3";
constexpr wchar_t kVersion[] = L"2.1.0";

// Layout in 96-DPI design units (scaled through AboutWindow::S()).
constexpr int kWindowWidth = 560;
constexpr int kWindowHeight = 520;
constexpr int kHeaderHeight = 48;
constexpr int kMargin = 24;
constexpr int kContentWidth = kWindowWidth - 2 * kMargin;
constexpr int kRowHeight = 24;
constexpr int kButtonWidth = 120;
constexpr int kButtonHeight = 34;
constexpr int kButtonY = 478;

constexpr int kTitleY = 84;
constexpr int kSourceY = 132;
constexpr int kRunCountY = 158;
constexpr int kIniPathY = 184;
constexpr int kDataDirY = 208;
constexpr int kContentLabelY = 240;
constexpr int kContentEditY = 266;
constexpr int kContentEditHeight = 154;

constexpr int kIdClose = IDOK;

}  // namespace

AboutWindow::AboutWindow(Settings& settings) : settings_(settings) {}

// Same resource-lifetime rule as ConfigWindow: the base destructor destroys the
// HWND after the dynamic type already degraded to Window, so GDI objects are
// released here explicitly.
AboutWindow::~AboutWindow() { DestroyResources(); }

bool AboutWindow::CreateAboutWindow(HINSTANCE instance) {
    if (!Create(instance, kWindowClass, Loc("resAboutTitle"),
                WS_POPUP, WS_EX_TOPMOST | WS_EX_TOOLWINDOW)) {
        return false;
    }
    const DWORD corner_preference = 2;  // DWMWCP_ROUND (Windows 11), best effort
    ::DwmSetWindowAttribute(hwnd_, 33, &corner_preference,
                            sizeof(corner_preference));
    PlaceOnCursorMonitor();
    return true;
}

int AboutWindow::S(int value) const {
    return ::MulDiv(value, dpi_, USER_DEFAULT_SCREEN_DPI);
}

void AboutWindow::CreateFonts() {
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
        title_log.lfHeight = -::MulDiv(17, dpi_, 72);
        title_log.lfWeight = FW_SEMIBOLD;
        title_font_ = ::CreateFontIndirectW(&title_log);
    }
}

void AboutWindow::DestroyResources() {
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

void AboutWindow::ApplyFont(HWND control) const {
    if (font_ != nullptr && control != nullptr) {
        ::SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font_), TRUE);
    }
}

HWND AboutWindow::MakeControl(const wchar_t* class_name, const wchar_t* text,
                              DWORD style, int x, int y, int width, int height,
                              int id) {
    HWND control = ::CreateWindowExW(
        0, class_name, text, style, S(x), S(y), S(width), S(height), hwnd_,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), instance_, nullptr);
    if (control == nullptr) {
        Logger::Instance().Warn(std::string("about window: cannot create ") +
                                ToUtf8(class_name));
        return nullptr;
    }
    ApplyFont(control);
    return control;
}

void AboutWindow::CreateControls() {
    title_text_ = MakeControl(WC_STATICW, L"", WS_CHILD | WS_VISIBLE | SS_LEFT,
                              kMargin + 52, kTitleY, kContentWidth - 52, 40, 0);
    if (title_text_ != nullptr) {
        ::SendMessageW(title_text_, WM_SETFONT,
                       reinterpret_cast<WPARAM>(title_font_ != nullptr ? title_font_
                                                                      : font_),
                       TRUE);
    }
    source_text_ = MakeControl(WC_STATICW, L"", WS_CHILD | WS_VISIBLE | SS_LEFT,
                               kMargin + 52, kSourceY, kContentWidth - 52,
                               kRowHeight, 0);
    run_count_text_ = MakeControl(WC_STATICW, L"", WS_CHILD | WS_VISIBLE | SS_LEFT,
                                  kMargin + 52, kRunCountY, kContentWidth - 52,
                                  kRowHeight, 0);
    ini_path_text_ = MakeControl(WC_STATICW, L"",
                                 WS_CHILD | WS_VISIBLE | SS_LEFT | SS_PATHELLIPSIS,
                                 kMargin + 52, kIniPathY, kContentWidth - 52,
                                 kRowHeight, 0);
    data_dir_text_ = MakeControl(WC_STATICW, L"",
                                 WS_CHILD | WS_VISIBLE | SS_LEFT | SS_PATHELLIPSIS,
                                 kMargin + 52, kDataDirY, kContentWidth - 52,
                                 kRowHeight, 0);
    content_label_ = MakeControl(WC_STATICW, L"", WS_CHILD | WS_VISIBLE | SS_LEFT,
                                 kMargin + 24, kContentLabelY,
                                 kContentWidth - 48, kRowHeight, 0);

    // Read-only multiline EDIT: scrolls, can take focus and lets the user
    // select the URLs in the built-in about text (a STATIC cannot).
    content_edit_ = MakeControl(
        WC_EDITW, L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER | WS_VSCROLL | ES_LEFT |
            ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
        kMargin + 24, kContentEditY, kContentWidth - 48, kContentEditHeight, 0);
    if (content_edit_ != nullptr) {
        ::SendMessageW(content_edit_, EM_SETMARGINS,
                       EC_LEFTMARGIN | EC_RIGHTMARGIN,
                       MAKELPARAM(S(8), S(8)));
        ::SetWindowSubclass(content_edit_, &AboutWindow::ChildSubclassProc, 2,
                            reinterpret_cast<DWORD_PTR>(this));
    }

    close_button_ = fluent::CreateButton(
        hwnd_, instance_, kIdClose, Loc("Common.BtnClose"), font_, kButtonWidth,
        kButtonHeight, dpi_, true);
    if (close_button_ != nullptr) {
        ::SetWindowPos(close_button_, nullptr,
                       S(kWindowWidth - kMargin - kButtonWidth), S(kButtonY), 0,
                       0, SWP_NOSIZE | SWP_NOZORDER);
        ::SetWindowSubclass(close_button_, &AboutWindow::ChildSubclassProc, 2,
                            reinterpret_cast<DWORD_PTR>(this));
    }
}

void AboutWindow::LoadAboutText() {
    if (content_edit_ == nullptr || content_label_ == nullptr) {
        return;
    }
    // About text is built in for all three locales; no About.<Lang> side file.
    ::SetWindowTextW(content_edit_,
                     Localization::Instance().Get("resAboutFallback").c_str());
    ::SetWindowTextW(content_label_,
                     Localization::Instance().Get("resAboutContent").c_str());
}

void AboutWindow::RefreshTexts() {
    if (hwnd_ == nullptr) {
        return;
    }
    const Localization& text = Localization::Instance();
    const std::wstring title =
        text.Get("resAboutTitle") + L" " + kVersion;    ::SetWindowTextW(title_text_, title.c_str());
    ::SetWindowTextW(source_text_, text.Get("resAboutSource").c_str());

    std::wstring count = text.Get("resAboutRunCount");
    const std::wstring token = L"%d";
    const size_t position = count.find(token);
    if (position != std::wstring::npos) {
        count.replace(position, token.size(),
                      std::to_wstring(settings_.shortcut_run_count));
    }
    ::SetWindowTextW(run_count_text_, count.c_str());

    std::wstring ini_path = settings_.ini_path;
    if (ini_path.empty()) {
        ini_path = ShellIntegration::ExecutableDirectory() + L"ALTRun.ini";
    }
    ::SetWindowTextW(ini_path_text_,
                     (text.Get("resAboutIniPath") + ini_path).c_str());
    ::SetWindowTextW(
        data_dir_text_,
        (text.Get("resAboutDataDir") + ShellIntegration::ExecutableDirectory())
            .c_str());

    LoadAboutText();
    ::InvalidateRect(hwnd_, nullptr, TRUE);
}

void AboutWindow::OnCreate() {
    dpi_ = static_cast<int>(::GetDpiForWindow(hwnd_));
    if (dpi_ <= 0) {
        dpi_ = USER_DEFAULT_SCREEN_DPI;
    }
    CreateFonts();
    background_brush_ = ::CreateSolidBrush(::GetSysColor(COLOR_BTNFACE));
    CreateControls();
    RefreshTexts();
    Logger::Instance().Trace("about window created");
}

void AboutWindow::Open() {
    if (hwnd_ == nullptr) {
        return;
    }
    RefreshTexts();
    PlaceOnCursorMonitor();
    ::ShowWindow(hwnd_, SW_SHOWNORMAL);
    ::SetForegroundWindow(hwnd_);
    if (close_button_ != nullptr) {
        ::SetFocus(close_button_);
    }
    Logger::Instance().Trace("about window opened");
}

void AboutWindow::PlaceOnCursorMonitor() {
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

void AboutWindow::DrawHeader(HDC dc) const {
    if (font_ == nullptr || title_font_ == nullptr) {
        return;
    }
    RECT client = {};
    ::GetClientRect(hwnd_, &client);
    fluent::FillBackground(dc, client);
    fluent::DrawTitleBar(dc, hwnd_, Loc("resAboutTitle"), title_font_,
                         S(kHeaderHeight));

    RECT card = {S(kMargin), S(64), S(kWindowWidth - kMargin), S(464)};
    fluent::DrawCard(dc, card, S(14));

    RECT footer = {S(kMargin), S(472), S(kWindowWidth - kMargin), S(504)};
    fluent::FillRounded(dc, footer, S(10), fluent::kSurface);
    fluent::StrokeRounded(dc, footer, S(10), fluent::kBorder);

    HICON icon = fluent::AppIcon();
    if (icon != nullptr) {
        ::DrawIconEx(dc, S(kMargin), S(84), icon, S(40), S(40), 0, nullptr,
                     DI_NORMAL);
    }
}

LRESULT AboutWindow::HandleMessage(UINT message, WPARAM wparam, LPARAM lparam) {
    const int title_height = S(kHeaderHeight);
    if (message == WM_NCHITTEST || message == WM_MOUSEMOVE ||
        message == WM_MOUSELEAVE || message == WM_LBUTTONUP) {
        const LRESULT chrome =
            fluent::HandleChromeMessage(hwnd_, message, wparam, lparam,
                                        title_height);
        if (chrome != -1) {
            return chrome;
        }
    }

    switch (message) {
        case WM_COMMAND:
            if (HIWORD(wparam) == BN_CLICKED && LOWORD(wparam) == kIdClose) {
                Hide();
                return 0;
            }
            return 0;
        case WM_CLOSE:
            // Closing means hide; the window is created once and reused.
            Hide();
            return 0;
        case WM_KEYDOWN:
            if (wparam == VK_ESCAPE || wparam == VK_RETURN) {
                Hide();
                return 0;
            }
            break;
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT: {
            PAINTSTRUCT paint = {};
            HDC dc = ::BeginPaint(hwnd_, &paint);
            DrawHeader(dc);
            ::EndPaint(hwnd_, &paint);
            return 0;
        }
        case WM_CTLCOLORSTATIC: {
            HDC dc = reinterpret_cast<HDC>(wparam);
            // The read-only multiline EDIT paints through WM_CTLCOLORSTATIC
            // too; keep it white so it reads like a document.
            if (reinterpret_cast<HWND>(lparam) == content_edit_) {
                ::SetBkColor(dc, fluent::kSurface);
                ::SetTextColor(dc, fluent::kText);
                return reinterpret_cast<LRESULT>(::GetStockObject(WHITE_BRUSH));
            }
            ::SetBkMode(dc, TRANSPARENT);
            ::SetTextColor(dc, fluent::kTextSecondary);
            return reinterpret_cast<LRESULT>(
                ::GetStockObject(HOLLOW_BRUSH));
        }
        case WM_CTLCOLOREDIT: {
            HDC dc = reinterpret_cast<HDC>(wparam);
            ::SetBkColor(dc, fluent::kSurface);
            ::SetTextColor(dc, fluent::kText);
            return reinterpret_cast<LRESULT>(::GetStockObject(WHITE_BRUSH));
        }
        case WM_DESTROY:
            // Also covered by the destructor; handled for an explicit Destroy()
            // while the object is still fully alive.
            DestroyResources();
            return 0;
        default:
            break;
    }
    return Window::HandleMessage(message, wparam, lparam);
}

LRESULT CALLBACK AboutWindow::ChildSubclassProc(HWND hwnd, UINT message,
                                                WPARAM wparam, LPARAM lparam,
                                                UINT_PTR, DWORD_PTR ref_data) {
    auto* self = reinterpret_cast<AboutWindow*>(ref_data);
    if (self == nullptr) {
        return ::DefSubclassProc(hwnd, message, wparam, lparam);
    }
    switch (message) {
        case WM_KEYDOWN: {
            const UINT vk = static_cast<UINT>(wparam);
            if (vk == VK_ESCAPE || vk == VK_RETURN) {
                ::PostMessageW(self->hwnd_, WM_CLOSE, 0, 0);
                return 0;
            }
            if (vk == VK_TAB) {
                // Only two focusable controls; Tab toggles between them.
                HWND target = hwnd == self->content_edit_ ? self->close_button_
                                                          : self->content_edit_;
                if (target != nullptr) {
                    ::SetFocus(target);
                }
                return 0;
            }
            break;
        }
        case WM_CHAR:
            if (wparam == L'\t' || wparam == L'\r' || wparam == L'\n' ||
                wparam == L'\x1b') {
                return 0;
            }
            break;
        default:
            break;
    }
    return ::DefSubclassProc(hwnd, message, wparam, lparam);
}

}  // namespace alt

