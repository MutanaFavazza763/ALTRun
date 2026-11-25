#include "ui/WelcomeWindow.h"

#include <commctrl.h>
#include <dwmapi.h>
#include <uxtheme.h>
#pragma comment(lib, "uxtheme.lib")
#include <windowsx.h>

#include <string>
#include <vector>

#include "core/Localization.h"
#include "core/Logger.h"
#include "core/Text.h"
#include "ui/FluentMenu.h"
#include "ui/FluentTheme.h"
#include "ui/UiText.h"

namespace alt {
namespace {

constexpr wchar_t kWindowClass[] = L"ALTRun.WelcomeWindow.v3";

// Layout in 96-DPI design units (scaled through WelcomeWindow::S()).
constexpr int kWindowWidth = 560;
constexpr int kWindowHeight = 520;
constexpr int kHeaderHeight = 56;
constexpr int kMargin = 24;
constexpr int kContentWidth = kWindowWidth - 2 * kMargin;

constexpr int kCardTop = 72;
constexpr int kCardBottom = 496;
constexpr int kIntroY = 92;
constexpr int kGuideY = 136;
constexpr int kGuideLineHeight = 22;
constexpr int kLanguageLabelY = 228;
constexpr int kLanguageComboY = 260;
constexpr int kAutoRunY = 304;
constexpr int kSendToY = 340;
constexpr int kNoteY = 376;
constexpr int kNoteHeight = 42;
constexpr int kButtonWidth = 184;
constexpr int kButtonHeight = 36;
constexpr int kButtonY = 428;

constexpr int kIdStart = IDOK;
constexpr int kIdLanguage = 3001;
constexpr int kIdLanguageFirst = 3100;

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
                WS_POPUP, WS_EX_TOPMOST | WS_EX_TOOLWINDOW)) {
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
        log_font.lfHeight = -::MulDiv(21, dpi_, 144);
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
        title_log.lfHeight = -::MulDiv(36, dpi_, USER_DEFAULT_SCREEN_DPI);
        title_log.lfWeight = FW_NORMAL;
        icon_font_ = ::CreateFontIndirectW(&title_log);

        LOGFONTW lead_log = {};
        ::GetObjectW(font_, static_cast<int>(sizeof(lead_log)), &lead_log);
        lead_log.lfWeight = FW_SEMIBOLD;
        lead_font_ = ::CreateFontIndirectW(&lead_log);

        LOGFONTW kbd_log = lead_log;
        kbd_log.lfHeight = -::MulDiv(20, dpi_, 144);
        kbd_font_ = ::CreateFontIndirectW(&kbd_log);
    }
}

void WelcomeWindow::DestroyResources() {
    if (kbd_font_ != nullptr) {
        ::DeleteObject(kbd_font_);
        kbd_font_ = nullptr;
    }
    if (lead_font_ != nullptr) {
        ::DeleteObject(lead_font_);
        lead_font_ = nullptr;
    }
    if (icon_font_ != nullptr) {
        ::DeleteObject(icon_font_);
        icon_font_ = nullptr;
    }
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
    intro_text_ = MakeControl(WC_STATICW, L"",
                              WS_CHILD | WS_VISIBLE | SS_LEFT | SS_CENTERIMAGE,
                              kMargin + 72, kIntroY, kContentWidth - 96, 30, 0);
    if (intro_text_ != nullptr && lead_font_ != nullptr) {
        ::SendMessageW(intro_text_, WM_SETFONT,
                       reinterpret_cast<WPARAM>(lead_font_), TRUE);
    }

    language_label_ = MakeControl(WC_STATICW, L"", WS_CHILD | WS_VISIBLE | SS_LEFT,
                                   kMargin + 24, kLanguageLabelY, 160, 24, 0);
    language_button_ = fluent::CreateButton(
        hwnd_, instance_, kIdLanguage, L"", font_, 200, 36, dpi_, false, true);
    if (language_button_ != nullptr) {
        ::SetWindowPos(language_button_, nullptr, S(kMargin + 24),
                       S(kLanguageComboY), 0, 0, SWP_NOSIZE | SWP_NOZORDER);
    }

    auto_run_check_ = fluent::CreateToggle(
        hwnd_, instance_, 0, L"", font_, kContentWidth - 48, 30, dpi_);
    send_to_check_ = fluent::CreateToggle(
        hwnd_, instance_, 0, L"", font_, kContentWidth - 48, 30, dpi_);
    if (auto_run_check_ != nullptr) {
        ::SetWindowPos(auto_run_check_, nullptr, S(kMargin + 24), S(kAutoRunY),
                       0, 0, SWP_NOSIZE | SWP_NOZORDER);
        ::SetWindowSubclass(auto_run_check_, &WelcomeWindow::ChildSubclassProc, 2,
                            reinterpret_cast<DWORD_PTR>(this));
    }
    if (send_to_check_ != nullptr) {
        ::SetWindowPos(send_to_check_, nullptr, S(kMargin + 24), S(kSendToY),
                       0, 0, SWP_NOSIZE | SWP_NOZORDER);
        ::SetWindowSubclass(send_to_check_, &WelcomeWindow::ChildSubclassProc, 2,
                            reinterpret_cast<DWORD_PTR>(this));
    }

    note_text_ = MakeControl(WC_STATICW, L"", WS_CHILD | WS_VISIBLE | SS_LEFT,
                             kMargin + 24, kNoteY, kContentWidth - 48,
                             kNoteHeight, 0);
    start_button_ = fluent::CreateButton(
        hwnd_, instance_, kIdStart, L"", font_, kButtonWidth, kButtonHeight, dpi_,
        true);
    if (start_button_ != nullptr) {
        ::SetWindowPos(start_button_, nullptr,
                       S((kWindowWidth - kButtonWidth) / 2), S(kButtonY), 0,
                       0, SWP_NOSIZE | SWP_NOZORDER);
        ::SetWindowSubclass(start_button_, &WelcomeWindow::ChildSubclassProc, 2,
                            reinterpret_cast<DWORD_PTR>(this));
    }
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
    guide_text_ = ReplaceToken(text.Get("resWelcomeGuide"), hotkey);
    ::SetWindowTextW(language_label_,
                     text.Get("resWelcomeLanguage").c_str());
    ::SetWindowTextW(auto_run_check_,
                     text.Get("resWelcomeAutoRun").c_str());
    ::SetWindowTextW(send_to_check_,
                     text.Get("resWelcomeSendTo").c_str());
    ::SetWindowTextW(note_text_, text.Get("resWelcomeNote").c_str());
    ::SetWindowTextW(start_button_, text.Get("resWelcomeStart").c_str());
    if (start_button_ != nullptr) {
        ::InvalidateRect(start_button_, nullptr, TRUE);
    }
    if (auto_run_check_ != nullptr) {
        ::InvalidateRect(auto_run_check_, nullptr, TRUE);
    }
    if (send_to_check_ != nullptr) {
        ::InvalidateRect(send_to_check_, nullptr, TRUE);
    }
    // The usage guide is painted by the parent (it mixes normal text with
    // keyboard-key pills), so force a repaint when the locale or hotkey changes.
    ::InvalidateRect(hwnd_, nullptr, TRUE);

    int index = 2;  // en_us
    if (settings_.lang == "zh_cn") {
        index = 0;
    } else if (settings_.lang == "zh_hk") {
        index = 1;
    }
    language_index_ = index;
    UpdateLanguageButton();
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
    fluent::FillBackground(dc, client);
    fluent::DrawTitleBar(dc, hwnd_, Loc("resWelcomeTitle"), title_font_,
                         S(kHeaderHeight));

    RECT card = {S(kMargin), S(kCardTop), S(kWindowWidth - kMargin),
                 S(kCardBottom)};
    fluent::DrawCard(dc, card, S(14));

    // Center the 40px tile on the 30px lead-text line so the star and the
    // headline share the same optical centre (kIntroY is the text top, the
    // line is 30px tall, so the tile starts 5px above it).
    RECT tile = {S(kMargin + 24), S(kIntroY - 5), S(kMargin + 64),
                 S(kIntroY + 35)};
    fluent::FillRounded(dc, tile, S(12), fluent::kAccentSoft);
    ::SetBkMode(dc, TRANSPARENT);
    ::SetTextColor(dc, fluent::kAccent);
    HGDIOBJ old = ::SelectObject(dc,
                                 icon_font_ != nullptr ? icon_font_ : title_font_);
    RECT glyph = tile;
    ::DrawTextW(dc, L"\u2726", -1, &glyph,
                DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
    ::SelectObject(dc, old);

    DrawGuide(dc);
}

void WelcomeWindow::DrawGuide(HDC dc) const {
    if (guide_text_.empty() || font_ == nullptr) {
        return;
    }
    ::SetBkMode(dc, TRANSPARENT);
    ::SetTextColor(dc, fluent::kText);
    HGDIOBJ old_font = ::SelectObject(dc, font_);
    const int left = S(kMargin + 24);
    const int top = S(kGuideY);
    const int line_height = S(kGuideLineHeight);
    size_t start = 0;
    int line_index = 0;
    while (start <= guide_text_.size()) {
        size_t end = guide_text_.find(L'\n', start);
        if (end == std::wstring::npos) {
            end = guide_text_.size();
        }
        std::wstring line = guide_text_.substr(start, end - start);
        if (!line.empty() && line.back() == L'\r') {
            line.pop_back();
        }
        DrawGuideLine(dc, line, left, top + line_index * line_height);
        ++line_index;
        if (end == guide_text_.size()) {
            break;
        }
        start = end + 1;
    }
    ::SelectObject(dc, old_font);
}

void WelcomeWindow::DrawGuideLine(HDC dc, const std::wstring& line, int x,
                                  int y) const {
    const int line_height = S(kGuideLineHeight);
    size_t position = 0;
    while (position < line.size()) {
        const size_t open = line.find(L"[[", position);
        const size_t normal_end =
            open == std::wstring::npos ? line.size() : open;
        if (normal_end > position) {
            const std::wstring normal = line.substr(position, normal_end - position);
            SIZE size = {};
            ::GetTextExtentPoint32W(dc, normal.c_str(),
                                    static_cast<int>(normal.size()), &size);
            ::TextOutW(dc, x, y + (line_height - size.cy) / 2, normal.c_str(),
                       static_cast<int>(normal.size()));
            x += size.cx;
        }
        if (open == std::wstring::npos) {
            break;
        }
        const size_t close = line.find(L"]]", open + 2);
        if (close == std::wstring::npos) {
            // Unclosed marker: draw the rest as normal text.
            const std::wstring remainder = line.substr(open);
            SIZE size = {};
            ::GetTextExtentPoint32W(dc, remainder.c_str(),
                                    static_cast<int>(remainder.size()), &size);
            ::TextOutW(dc, x, y + (line_height - size.cy) / 2,
                       remainder.c_str(), static_cast<int>(remainder.size()));
            break;
        }
        const std::wstring key = line.substr(open + 2, close - open - 2);
        if (!key.empty() && kbd_font_ != nullptr) {
            HGDIOBJ old_key_font = ::SelectObject(dc, kbd_font_);
            SIZE key_size = {};
            ::GetTextExtentPoint32W(dc, key.c_str(),
                                    static_cast<int>(key.size()), &key_size);
            const int padding_x = S(7);
            const int pill_height = S(20);
            const int pill_top = y + (line_height - pill_height) / 2;
            const int pill_width = key_size.cx + padding_x * 2;
            RECT pill = {x, pill_top, x + pill_width, pill_top + pill_height};
            fluent::FillRounded(dc, pill, S(5), RGB(241, 244, 248));
            fluent::StrokeRounded(dc, pill, S(5), RGB(214, 220, 229));
            ::SetTextColor(dc, fluent::kText);
            ::TextOutW(dc, x + padding_x,
                       pill_top + (pill_height - key_size.cy) / 2, key.c_str(),
                       static_cast<int>(key.size()));
            ::SelectObject(dc, old_key_font);
            x += pill_width + S(3);
        } else if (!key.empty()) {
            SIZE size = {};
            ::GetTextExtentPoint32W(dc, key.c_str(),
                                    static_cast<int>(key.size()), &size);
            ::TextOutW(dc, x, y + (line_height - size.cy) / 2, key.c_str(),
                       static_cast<int>(key.size()));
            x += size.cx;
        }
        position = close + 2;
    }
}

void WelcomeWindow::UpdateLanguageButton() {
    if (language_button_ == nullptr) {
        return;
    }
    std::wstring label = kLanguageNames[language_index_];
    ::SetWindowTextW(language_button_, label.c_str());
    ::InvalidateRect(language_button_, nullptr, TRUE);
}

void WelcomeWindow::ShowLanguageMenu() {
    if (language_button_ == nullptr) {
        return;
    }
    RECT button = {};
    ::GetWindowRect(language_button_, &button);
    POINT point = {button.left, button.bottom + 4};
    std::vector<fluent::MenuItem> items;
    for (int index = 0; index < 3; ++index) {
        std::wstring label = index == language_index_ ? L"\u2713  " : L"     ";
        label += kLanguageNames[index];
        items.push_back({kIdLanguageFirst + index, label, L"", false, false,
                         true});
    }
    const int command = fluent::ShowPopupMenu(
        hwnd_, instance_, point, Loc("resWelcomeLanguage"), L"", nullptr, items);
    if (command >= kIdLanguageFirst && command < kIdLanguageFirst + 3) {
        language_index_ = command - kIdLanguageFirst;
        UpdateLanguageButton();
    }
}

void WelcomeWindow::Accept() {
    int index = language_index_;
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
            if (HIWORD(wparam) == BN_CLICKED && LOWORD(wparam) == kIdStart) {
                Accept();
                return 0;
            }
            if (HIWORD(wparam) == BN_CLICKED && LOWORD(wparam) == kIdLanguage) {
                ShowLanguageMenu();
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
            HWND control = reinterpret_cast<HWND>(lparam);
            ::SetBkMode(dc, TRANSPARENT);
            ::SetTextColor(dc, control == intro_text_ ? fluent::kAccent
                                                      : fluent::kText);
            return reinterpret_cast<LRESULT>(
                ::GetStockObject(HOLLOW_BRUSH));
        }
        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORLISTBOX: {
            HDC dc = reinterpret_cast<HDC>(wparam);
            ::SetBkColor(dc, fluent::kSurface);
            ::SetTextColor(dc, fluent::kText);
            return reinterpret_cast<LRESULT>(::GetStockObject(WHITE_BRUSH));
        }
        case WM_DESTROY:
            DestroyResources();
            return 0;
        default:
            break;
    }
    return Window::HandleMessage(message, wparam, lparam);
}

LRESULT CALLBACK WelcomeWindow::ChildSubclassProc(HWND hwnd, UINT message,
                                                  WPARAM wparam, LPARAM lparam,
                                                  UINT_PTR, DWORD_PTR ref_data) {
    auto* self = reinterpret_cast<WelcomeWindow*>(ref_data);
    if (self == nullptr) {
        return ::DefSubclassProc(hwnd, message, wparam, lparam);
    }
    if (message == WM_KEYDOWN && wparam == VK_ESCAPE) {
        ::PostMessageW(self->hwnd_, WM_CLOSE, 0, 0);
        return 0;
    }
    if (message == WM_CHAR && wparam == L'\x1b') {
        return 0;
    }
    return ::DefSubclassProc(hwnd, message, wparam, lparam);
}

}  // namespace alt

