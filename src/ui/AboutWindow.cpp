#include "ui/AboutWindow.h"

#include <dwmapi.h>

#include <string>

#include "app/ShellIntegration.h"
#include "core/Localization.h"
#include "core/Logger.h"
#include "core/Text.h"
#include "ui/UiText.h"

namespace alt {
namespace {

constexpr wchar_t kWindowClass[] = L"ALTRun.AboutWindow.v2";
constexpr wchar_t kVersion[] = L"2.0 (C++ rewrite)";

// Layout in 96-DPI design units (scaled through AboutWindow::S()).
constexpr int kWindowWidth = 420;
constexpr int kWindowHeight = 360;
constexpr int kHeaderHeight = 44;
constexpr int kMargin = 16;
constexpr int kContentWidth = kWindowWidth - 2 * kMargin;
constexpr int kRowHeight = 22;
constexpr int kButtonWidth = 84;
constexpr int kButtonHeight = 30;

constexpr int kTitleY = 52;
constexpr int kSourceY = 82;
constexpr int kRunCountY = 104;
constexpr int kIniPathY = 126;
constexpr int kDataDirY = 148;
constexpr int kContentLabelY = 172;
constexpr int kContentEditY = 194;
constexpr int kContentEditHeight = 112;
constexpr int kButtonY = 316;

constexpr int kIdClose = IDOK;

std::wstring NormalizeNewlines(std::wstring text) {
    std::wstring normalized;
    normalized.reserve(text.size() + text.size() / 8);
    for (size_t index = 0; index < text.size(); ++index) {
        const wchar_t character = text[index];
        if (character == L'\r') {
            normalized.push_back(L'\r');
            if (index + 1 < text.size() && text[index + 1] == L'\n') {
                normalized.push_back(L'\n');
                ++index;
            } else {
                normalized.push_back(L'\n');
            }
        } else if (character == L'\n') {
            normalized += L"\r\n";
        } else {
            normalized.push_back(character);
        }
    }
    return normalized;
}

}  // namespace

AboutWindow::AboutWindow(Settings& settings) : settings_(settings) {}

// Same resource-lifetime rule as ConfigWindow: the base destructor destroys the
// HWND after the dynamic type already degraded to Window, so GDI objects are
// released here explicitly.
AboutWindow::~AboutWindow() { DestroyResources(); }

bool AboutWindow::CreateAboutWindow(HINSTANCE instance) {
    if (!Create(instance, kWindowClass, Loc("resAboutTitle"),
                WS_POPUP | WS_CLIPCHILDREN,
                WS_EX_TOPMOST | WS_EX_TOOLWINDOW)) {
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
        title_log.lfHeight = static_cast<LONG>(title_log.lfHeight * 1.35);
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
                              kMargin, kTitleY, kContentWidth, 28, 0);
    if (title_text_ != nullptr) {
        ::SendMessageW(title_text_, WM_SETFONT,
                       reinterpret_cast<WPARAM>(title_font_ != nullptr ? title_font_
                                                                      : font_),
                       TRUE);
    }
    source_text_ = MakeControl(WC_STATICW, L"", WS_CHILD | WS_VISIBLE | SS_LEFT,
                               kMargin, kSourceY, kContentWidth, kRowHeight, 0);
    run_count_text_ = MakeControl(WC_STATICW, L"", WS_CHILD | WS_VISIBLE | SS_LEFT,
                                  kMargin, kRunCountY, kContentWidth, kRowHeight, 0);
    ini_path_text_ = MakeControl(WC_STATICW, L"",
                                 WS_CHILD | WS_VISIBLE | SS_LEFT | SS_PATHELLIPSIS,
                                 kMargin, kIniPathY, kContentWidth, kRowHeight, 0);
    data_dir_text_ = MakeControl(WC_STATICW, L"",
                                 WS_CHILD | WS_VISIBLE | SS_LEFT | SS_PATHELLIPSIS,
                                 kMargin, kDataDirY, kContentWidth, kRowHeight, 0);
    content_label_ = MakeControl(WC_STATICW, L"", WS_CHILD | WS_VISIBLE | SS_LEFT,
                                 kMargin, kContentLabelY, kContentWidth, kRowHeight,
                                 0);

    // Read-only multiline EDIT: scrolls, can take focus and lets the user
    // select the URLs in About.<Lang> (a STATIC cannot).
    content_edit_ = MakeControl(
        WC_EDITW, L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | ES_LEFT | ES_MULTILINE |
            ES_READONLY | ES_AUTOVSCROLL,
        kMargin, kContentEditY, kContentWidth, kContentEditHeight, 0);
    if (content_edit_ != nullptr) {
        ::SendMessageW(content_edit_, EM_SETMARGINS,
                       EC_LEFTMARGIN | EC_RIGHTMARGIN,
                       MAKELPARAM(S(4), S(4)));
        ::SetWindowSubclass(content_edit_, &AboutWindow::ChildSubclassProc, 1,
                            reinterpret_cast<DWORD_PTR>(this));
    }

    close_button_ = MakeControl(
        WC_BUTTONW, Loc("Common.BtnClose").c_str(),
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
        kWindowWidth - kMargin - kButtonWidth, kButtonY, kButtonWidth,
        kButtonHeight, kIdClose);
    if (close_button_ != nullptr) {
        ::SetWindowSubclass(close_button_, &AboutWindow::ChildSubclassProc, 1,
                            reinterpret_cast<DWORD_PTR>(this));
    }
}

std::wstring AboutWindow::AboutFilePath() const {
    std::string lang = settings_.lang.empty() ? std::string("EN") : settings_.lang;
    return ShellIntegration::ExecutableDirectory() + L"About." + ToWide(lang);
}

void AboutWindow::LoadAboutText() {
    if (content_edit_ == nullptr || content_label_ == nullptr) {
        return;
    }
    const std::wstring path = AboutFilePath();
    std::string content;
    const bool found = Localization::ReadTextFileUtf8(path, &content);
    std::wstring text;
    if (found) {
        text = NormalizeNewlines(ToWide(content));
        Logger::Instance().Trace("about: loaded " + ToUtf8(path));
    } else {
        text = Localization::Instance().Get("resAboutFallback");
        Logger::Instance().Trace("about: " + ToUtf8(path) +
                                 " not found, using built-in text");
    }
    ::SetWindowTextW(content_edit_, text.c_str());

    // Only the file name is shown: the full exe directory is already in the
    // "Data directory" line and a long path would ellipsize the state suffix.
    std::string lang = settings_.lang.empty() ? std::string("EN") : settings_.lang;
    std::wstring label = Localization::Instance().Get("resAboutContent") +
                         L"About." + ToWide(lang);
    if (!found) {
        label += Localization::Instance().Get("resAboutContentMissing");
    }
    ::SetWindowTextW(content_label_, label.c_str());
}

void AboutWindow::RefreshTexts() {
    if (hwnd_ == nullptr) {
        return;
    }
    const Localization& text = Localization::Instance();
    const std::wstring title =
        text.Get("resAboutTitle") + L" " + kVersion;
    ::SetWindowTextW(title_text_, title.c_str());
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

    ::SetBkMode(dc, TRANSPARENT);
    ::SetTextColor(dc, ::GetSysColor(COLOR_BTNTEXT));
    HGDIOBJ previous = ::SelectObject(dc, title_font_);
    RECT title = {S(20), S(4), client.right - S(20), S(kHeaderHeight - 6)};
    const std::wstring header = Loc("resAboutTitle");
    ::DrawTextW(dc, header.c_str(), -1, &title,
                DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
    ::SelectObject(dc, previous);

    const RECT separator = {S(16), S(kHeaderHeight), client.right - S(16),
                            S(kHeaderHeight) + 1};
    ::FillRect(dc, &separator, ::GetSysColorBrush(COLOR_3DSHADOW));
}

LRESULT AboutWindow::HandleMessage(UINT message, WPARAM wparam, LPARAM lparam) {
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
            // The read-only multiline EDIT paints through WM_CTLCOLORSTATIC
            // too; keep it white so it reads like a document.
            if (reinterpret_cast<HWND>(lparam) == content_edit_) {
                ::SetBkColor(dc, ::GetSysColor(COLOR_WINDOW));
                ::SetTextColor(dc, ::GetSysColor(COLOR_WINDOWTEXT));
                return reinterpret_cast<LRESULT>(::GetSysColorBrush(COLOR_WINDOW));
            }
            ::SetBkMode(dc, TRANSPARENT);
            return reinterpret_cast<LRESULT>(background_brush_);
        }
        case WM_CTLCOLOREDIT: {
            HDC dc = reinterpret_cast<HDC>(wparam);
            ::SetBkColor(dc, ::GetSysColor(COLOR_WINDOW));
            ::SetTextColor(dc, ::GetSysColor(COLOR_WINDOWTEXT));
            return reinterpret_cast<LRESULT>(::GetSysColorBrush(COLOR_WINDOW));
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
