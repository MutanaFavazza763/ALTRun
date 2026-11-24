#include "ui/ConfigWindow.h"

#include <commctrl.h>
#include <dwmapi.h>

#include <algorithm>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

#include "core/Logger.h"
#include "core/Text.h"
#include "ui/UiText.h"

namespace alt {
namespace {

constexpr wchar_t kWindowClass[] = L"ALTRun.ConfigWindow.v2";
// Version string shown on the 调试/关于 tab (single source of truth; the
// original read about text from About.<Lang>, which the rewrite does not ship).
constexpr wchar_t kVersion[] = L"ALTRun C++ rewrite (dev)";

// Layout is written in 96-DPI design units and scaled with MulDiv in
// ConfigWindow::S(); this keeps the dialog usable on >100% DPI monitors.
constexpr int kWindowWidth = 720;
constexpr int kWindowHeight = 540;
constexpr int kMargin = 16;
constexpr int kHeaderHeight = 44;

constexpr int kTabX = 16;
constexpr int kTabY = 44;
constexpr int kTabWidth = 688;
constexpr int kTabHeight = 438;

constexpr int kButtonWidth = 84;
constexpr int kButtonHeight = 30;
constexpr int kButtonY = 494;
constexpr int kButtonGap = 8;

// Top-left corner of the page content, in design units (the tab control's own
// client area starts below the tab strip).
constexpr int kPageLeft = kTabX + 12;
constexpr int kPageTop = kTabY + 34;

constexpr int kRowHeight = 24;
constexpr int kRowStep = 28;
constexpr int kLabelGap = 8;
constexpr int kHotkeyStep = 40;

constexpr int kCheckColumnWidth = 320;
constexpr int kCheckColumnOffset = 340;

// ---------------------------------------------------------------------------
// Control <-> Settings field descriptors. The tab pages are built by walking
// these tables, which is what keeps LoadFromSettings()/SaveFromControls()
// short and hard to get out of sync.
// ---------------------------------------------------------------------------

struct CheckSpec {
    const char* key;  // legacy .lang key, resolved through Localization
    bool Settings::*field;
};

struct IntSpec {
    const char* key;
    int Settings::*field;
    int min_value;
    int max_value;
};

// [Config] page 1 -- 13 checkboxes (task list order).  The legacy dialog had the
// same list as ConfigForm.ConfigList_0..19, so the keys are reused verbatim.
const CheckSpec kGeneralChecks[] = {
    {"ConfigForm.ConfigList_2", &Settings::regex_mode},
    {"ConfigForm.ConfigList_3", &Settings::match_anywhere},
    {"ConfigForm.ConfigList_4", &Settings::number_key},
    {"ConfigForm.ConfigList_5", &Settings::index_from_0_to_9},
    {"ConfigForm.ConfigList_6", &Settings::remember_favourite_match},
    {"ConfigForm.ConfigList_10", &Settings::show_top_ten},
    {"ConfigForm.ConfigList_19", &Settings::execute_if_only_one},
    {"ConfigForm.ConfigList_12", &Settings::exit_when_execute},
    {"ConfigForm.ConfigList_8", &Settings::show_command_line},
    {"ConfigForm.ConfigList_7", &Settings::show_operation_hint},
    {"ConfigForm.ConfigList_0", &Settings::auto_run},
    {"ConfigForm.ConfigList_1", &Settings::add_to_send_to},
    {"ConfigForm.ConfigList_14", &Settings::show_me_when_start},
};

// [Config] numeric keys that the legacy ini keeps but the old dialog had no
// widget for (docs/spec/02-config-options.md §1.3).
const IntSpec kGeneralInts[] = {
    {"resHideDelay", &Settings::hide_delay, 0, 3600},
    {"resParamHistoryLimit", &Settings::param_history_limit, 0, 1000},
};

const CheckSpec kAppearanceChecks[] = {
    {"ConfigForm.ConfigList_13", &Settings::show_skin},
    {"ConfigForm.ConfigList_15", &Settings::show_tray_icon},
    {"ConfigForm.ConfigList_11", &Settings::play_popup_notify},
    {"ConfigForm.ConfigList_9", &Settings::show_start_notification},
};

// Tab strip.  The legacy dialog had 配置/热键/字体/窗体/语言; the rewrite folds
// font/form/language into one 外观 page and adds a debug page, so only the
// first two tabs map 1:1 to a legacy key.
const char* const kTabKeys[] = {"ConfigForm.PageConfig", "ConfigForm.PageHotKey",
                                "ConfigForm.PageForm", "resPageDebug"};

// ---------------------------------------------------------------------------
// Small helpers
// ---------------------------------------------------------------------------

std::wstring GetControlText(HWND control) {
    const int length = ::GetWindowTextLengthW(control);
    if (length <= 0) {
        return {};
    }
    std::wstring text(static_cast<size_t>(length) + 1, L'\0');
    const int copied = ::GetWindowTextW(control, text.data(), length + 1);
    if (copied <= 0) {
        return {};
    }
    text.resize(static_cast<size_t>(copied));
    return text;
}

void SetControlText(HWND control, const std::wstring& text) {
    if (control != nullptr) {
        ::SetWindowTextW(control, text.c_str());
    }
}

// Tolerant integer reader: empty or non-numeric text keeps the current value,
// so a half-edited field never throws or resets itself to 0.
int ReadIntControl(HWND control, int fallback) {
    const std::wstring text = GetControlText(control);
    const size_t first = text.find_first_not_of(L" \t\r\n");
    if (first == std::wstring::npos) {
        return fallback;
    }
    const size_t last = text.find_last_not_of(L" \t\r\n");
    const std::wstring trimmed = text.substr(first, last - first + 1);

    size_t index = 0;
    bool negative = false;
    if (trimmed[0] == L'-' || trimmed[0] == L'+') {
        negative = trimmed[0] == L'-';
        index = 1;
    }
    if (index >= trimmed.size()) {
        return fallback;
    }
    long long value = 0;
    for (; index < trimmed.size(); ++index) {
        const wchar_t character = trimmed[index];
        if (character < L'0' || character > L'9') {
            return fallback;
        }
        value = value * 10 + static_cast<long long>(character - L'0');
        if (value > 1000000) {
            return fallback;  // absurdly large: treat as invalid input
        }
    }
    if (negative) {
        value = -value;
    }
    return static_cast<int>(value);
}

bool IsFocusable(HWND control) {
    return (::GetWindowLongPtrW(control, GWL_STYLE) & WS_TABSTOP) != 0;
}

bool IsModifierKey(UINT vk) {
    switch (vk) {
        case VK_SHIFT:
        case VK_LSHIFT:
        case VK_RSHIFT:
        case VK_CONTROL:
        case VK_LCONTROL:
        case VK_RCONTROL:
        case VK_MENU:
        case VK_LMENU:
        case VK_RMENU:
        case VK_LWIN:
        case VK_RWIN:
            return true;
        default:
            return false;
    }
}

// Translates a virtual key into the key names accepted by ParseHotkey()
// (src/app/Hotkey.cpp), so captured text always round-trips through the ini.
std::wstring KeyName(UINT vk) {
    if ((vk >= 'A' && vk <= 'Z') || (vk >= '0' && vk <= '9')) {
        return std::wstring(1, static_cast<wchar_t>(vk));
    }
    if (vk >= VK_F1 && vk <= VK_F24) {
        return L"F" + std::to_wstring(static_cast<unsigned>(vk - VK_F1 + 1));
    }
    switch (vk) {
        case VK_PAUSE:
            return L"Pause";
        case VK_SPACE:
            return L"Space";
        case VK_RETURN:
            return L"Enter";
        case VK_TAB:
            return L"Tab";
        case VK_INSERT:
            return L"Insert";
        case VK_DELETE:
            return L"Delete";
        case VK_HOME:
            return L"Home";
        case VK_END:
            return L"End";
        case VK_PRIOR:
            return L"PageUp";
        case VK_NEXT:
            return L"PageDown";
        case VK_LEFT:
            return L"Left";
        case VK_RIGHT:
            return L"Right";
        case VK_UP:
            return L"Up";
        case VK_DOWN:
            return L"Down";
        case VK_SNAPSHOT:
            return L"PrintScreen";
        case VK_SCROLL:
            return L"ScrollLock";
        case VK_NUMLOCK:
            return L"NumLock";
        case VK_CAPITAL:
            return L"CapsLock";
        case VK_OEM_3:
            return L"`";
        default:
            return {};
    }
}

// Modifier order chosen by this rewrite: Ctrl -> Alt -> Shift -> Win (the old
// dialog emitted Win, Alt, Ctrl, Shift; ParseHotkey is order insensitive so
// both forms load fine).
std::wstring FormatHotkey(UINT vk, bool ctrl, bool alt, bool shift, bool win) {
    const std::wstring name = KeyName(vk);
    if (name.empty()) {
        return {};
    }
    std::wstring text;
    if (ctrl) {
        text += L"Ctrl + ";
    }
    if (alt) {
        text += L"Alt + ";
    }
    if (shift) {
        text += L"Shift + ";
    }
    if (win) {
        text += L"Win + ";
    }
    text += name;
    return text;
}

std::wstring ExeDirectory() {
    wchar_t path[MAX_PATH * 2] = {};
    ::GetModuleFileNameW(nullptr, path, static_cast<DWORD>(std::size(path)));
    std::wstring text = path;
    const size_t slash = text.find_last_of(L"\\/");
    if (slash != std::wstring::npos) {
        text.resize(slash + 1);
    }
    return text;
}

}  // namespace

// ---------------------------------------------------------------------------
// Lifetime
// ---------------------------------------------------------------------------

ConfigWindow::ConfigWindow(Settings& settings) : settings_(settings) {}

// GDI objects are released before ~Window() runs DestroyWindow(): while the
// base destructor executes the dynamic type is already Window, so the
// WM_DESTROY branch below would not reach this class' handler.
ConfigWindow::~ConfigWindow() { DestroyResources(); }

void ConfigWindow::DestroyResources() {
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

bool ConfigWindow::CreateConfigWindow(HINSTANCE instance) {
    INITCOMMONCONTROLSEX controls = {};
    controls.dwSize = static_cast<DWORD>(sizeof(controls));
    controls.dwICC = ICC_TAB_CLASSES;
    ::InitCommonControlsEx(&controls);

    // WS_CLIPCHILDREN keeps the native child controls from being painted over
    // by the parent background erase; the popup/tool-window/topmost combination
    // matches MainWindow and ParamWindow.
    if (!Create(instance, kWindowClass,
                L"ALTRun " + Loc("ConfigForm.ConfigFormCaption"),
                WS_POPUP | WS_CLIPCHILDREN, WS_EX_TOPMOST | WS_EX_TOOLWINDOW)) {
        return false;
    }

    const DWORD corner_preference = 2;  // DWMWCP_ROUND (Windows 11)
    ::DwmSetWindowAttribute(hwnd_, 33, &corner_preference,
                            sizeof(corner_preference));

    PlaceOnCursorMonitor();
    return true;
}

void ConfigWindow::SetApplyHandler(std::function<void()> handler) {
    apply_handler_ = std::move(handler);
}

// ---------------------------------------------------------------------------
// Creation
// ---------------------------------------------------------------------------

int ConfigWindow::S(int value) const {
    return ::MulDiv(value, dpi_, USER_DEFAULT_SCREEN_DPI);
}

void ConfigWindow::CreateFonts() {
    NONCLIENTMETRICSW metrics = {};
    metrics.cbSize = static_cast<UINT>(sizeof(metrics));
    if (::SystemParametersInfoW(SPI_GETNONCLIENTMETRICS,
                                static_cast<UINT>(sizeof(metrics)), &metrics,
                                0) != FALSE) {
        LOGFONTW log_font = metrics.lfMessageFont;
        // 9pt at the window's DPI; the message font itself is already the
        // system UI font (Segoe UI / Microsoft YaHei UI depending on locale).
        log_font.lfHeight = -::MulDiv(9, dpi_, 72);
        font_ = ::CreateFontIndirectW(&log_font);
    }
    if (font_ == nullptr) {
        font_ = static_cast<HFONT>(::GetStockObject(DEFAULT_GUI_FONT));
        font_is_stock_ = true;
    }

    LOGFONTW title_log = {};
    if (::GetObjectW(font_, static_cast<int>(sizeof(title_log)), &title_log) != 0) {
        title_log.lfHeight = static_cast<LONG>(title_log.lfHeight * 1.3);
        title_log.lfWeight = FW_SEMIBOLD;
        title_font_ = ::CreateFontIndirectW(&title_log);
    }
}

void ConfigWindow::ApplyFont(HWND control) const {
    if (font_ != nullptr) {
        ::SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font_), TRUE);
    }
}

HWND ConfigWindow::MakeControl(int page, const wchar_t* class_name,
                               const std::wstring& text, DWORD style, int x,
                               int y, int width, int height, int id) {
    HWND control = ::CreateWindowExW(
        0, class_name, text.c_str(), style, S(x), S(y), S(width), S(height),
        hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), instance_,
        nullptr);
    if (control == nullptr) {
        Logger::Instance().Warn(std::string("config window: cannot create ") +
                                ToUtf8(class_name));
        return nullptr;
    }
    ApplyFont(control);
    if (page >= 0 && page < kPageCount) {
        page_controls_[static_cast<size_t>(page)].push_back(control);
    }
    return control;
}

HWND ConfigWindow::AddLabel(int page, const std::wstring& text, int x, int y,
                            int width, int height, DWORD extra_style) {
    return MakeControl(page, WC_STATICW, text,
                       WS_CHILD | SS_CENTERIMAGE | extra_style, x, y, width,
                       height, 0);
}

HWND ConfigWindow::AddCheckBox(int page, const std::wstring& text, int x,
                               int y, int width) {
    HWND box = MakeControl(page, WC_BUTTONW, text,
                           WS_CHILD | WS_TABSTOP | BS_AUTOCHECKBOX, x, y, width,
                           kRowHeight, 0);
    if (box != nullptr) {
        ::SetWindowSubclass(box, &ConfigWindow::ChildSubclassProc, 1,
                            reinterpret_cast<DWORD_PTR>(this));
    }
    return box;
}

HWND ConfigWindow::AddIntEdit(int page, const std::wstring& label,
                              int Settings::*field, int min_value, int max_value,
                              int x, int y, int label_width, int edit_width) {
    AddLabel(page, label, x, y, label_width);
    HWND edit = MakeControl(page, WC_EDITW, L"",
                            WS_CHILD | WS_TABSTOP | ES_LEFT | ES_NUMBER |
                                ES_AUTOHSCROLL,
                            x + label_width + kLabelGap, y, edit_width,
                            kRowHeight, 0);
    if (edit != nullptr) {
        ints_.push_back(IntBinding{edit, field, min_value, max_value});
        ::SendMessageW(edit, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN,
                       MAKELPARAM(S(4), S(4)));
        ::SetWindowSubclass(edit, &ConfigWindow::ChildSubclassProc, 1,
                            reinterpret_cast<DWORD_PTR>(this));
    }
    return edit;
}

HWND ConfigWindow::AddTextEdit(int page, const std::wstring& label,
                               std::string Settings::*field, int x, int y,
                               int label_width, int edit_width) {
    AddLabel(page, label, x, y, label_width);
    HWND edit = MakeControl(page, WC_EDITW, L"",
                            WS_CHILD | WS_TABSTOP | ES_LEFT | ES_AUTOHSCROLL, x +
                                label_width + kLabelGap,
                            y, edit_width, kRowHeight, 0);
    if (edit != nullptr) {
        texts_.push_back(TextBinding{edit, field});
        ::SendMessageW(edit, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN,
                       MAKELPARAM(S(4), S(4)));
        ::SetWindowSubclass(edit, &ConfigWindow::ChildSubclassProc, 1,
                            reinterpret_cast<DWORD_PTR>(this));
    }
    return edit;
}

HWND ConfigWindow::AddHotkeyEdit(int page, const std::wstring& label,
                                 std::string Settings::*field, int x, int y,
                                 int label_width, int edit_width) {
    AddLabel(page, label, x, y, label_width);
    // Read-only EDIT instead of STATIC: it can take focus, shows the captured
    // text with the normal edit caret/selection and Theme support, while the
    // HotkeySubclassProc consumes every key press.
    HWND edit = MakeControl(page, WC_EDITW, L"",
                            WS_CHILD | WS_TABSTOP | ES_LEFT | ES_AUTOHSCROLL |
                                ES_READONLY,
                            x + label_width + kLabelGap, y, edit_width,
                            kRowHeight, 0);
    if (edit != nullptr) {
        texts_.push_back(TextBinding{edit, field});
        ::SendMessageW(edit, EM_SETCUEBANNER, TRUE,
                       reinterpret_cast<LPARAM>(Loc("resHotkeyCue").c_str()));
        ::SendMessageW(edit, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN,
                       MAKELPARAM(S(4), S(4)));
        ::SetWindowSubclass(edit, &ConfigWindow::HotkeySubclassProc, 1,
                            reinterpret_cast<DWORD_PTR>(this));
    }
    return edit;
}

void ConfigWindow::CreateControls() {
    CreateGeneralPage();
    CreateHotkeyPage();
    CreateAppearancePage();
    CreateAboutPage();
}

void ConfigWindow::CreateGeneralPage() {
    const int check_count = static_cast<int>(std::size(kGeneralChecks));
    constexpr int kChecksPerColumn = 7;
    for (int index = 0; index < check_count; ++index) {
        const int column = index < kChecksPerColumn ? 0 : 1;
        const int row = index < kChecksPerColumn ? index : index - kChecksPerColumn;
        HWND box = AddCheckBox(kPageGeneral, Loc(kGeneralChecks[index].key),
                               kPageLeft + column * kCheckColumnOffset,
                               kPageTop + 8 + row * kRowStep,
                               kCheckColumnWidth);
        if (box != nullptr) {
            bools_.push_back(BoolBinding{box, kGeneralChecks[index].field});
        }
    }

    const int int_count = static_cast<int>(std::size(kGeneralInts));
    for (int index = 0; index < int_count; ++index) {
        AddIntEdit(kPageGeneral, Loc(kGeneralInts[index].key),
                   kGeneralInts[index].field, kGeneralInts[index].min_value,
                   kGeneralInts[index].max_value, kPageLeft,
                   kPageTop + 216 + index * 32, 140, 80);
    }

    AddLabel(kPageGeneral, Loc("resGeneralPageHint"), kPageLeft,
             kPageTop + 296, 620);
}

void ConfigWindow::CreateHotkeyPage() {
    // Hotkey capture writes directly into the text box only; Settings is
    // touched when 确定 is pressed (single write-back point, see class comment).
    AddHotkeyEdit(kPageHotkey, Loc("ConfigForm.LblHotKey1"), &Settings::hotkey1,
                  kPageLeft, kPageTop + 8, 200, 380);
    AddHotkeyEdit(kPageHotkey, Loc("ConfigForm.LblHotKey2"), &Settings::hotkey2,
                  kPageLeft, kPageTop + 8 + kHotkeyStep, 200, 380);
    AddHotkeyEdit(kPageHotkey, Loc("resLastItemHotKey"),
                  &Settings::last_item_hotkey, kPageLeft,
                  kPageTop + 8 + 2 * kHotkeyStep, 200, 380);

    AddLabel(kPageHotkey, Loc("resHotkeyHint1"), kPageLeft, kPageTop + 144,
             640);
    AddLabel(kPageHotkey, Loc("resHotkeyHint2"), kPageLeft, kPageTop + 168,
             640);
    AddLabel(kPageHotkey, Loc("resHotkeyHint3"), kPageLeft, kPageTop + 192,
             640);
}

void ConfigWindow::CreateAppearancePage() {
    AddIntEdit(kPageAppearance, Loc("ConfigForm.lblFormWidth"),
               &Settings::form_width, 200, 800, kPageLeft, kPageTop + 8, 120,
               80);
    AddIntEdit(kPageAppearance, Loc("ConfigForm.LblFormAlpha"), &Settings::alpha,
               0, 255, kPageLeft, kPageTop + 44, 120, 80);
    AddIntEdit(kPageAppearance, Loc("ConfigForm.lblRoundBorderRadius"),
               &Settings::round_border_radius, 0, 200, kPageLeft,
               kPageTop + 80, 120, 80);
    AddTextEdit(kPageAppearance, Loc("ConfigForm.LblLanguage"), &Settings::lang,
                kPageLeft, kPageTop + 116, 120, 80);
    AddTextEdit(kPageAppearance, Loc("ConfigForm.LblBackGroundImage"),
                &Settings::bg_file_name, kPageLeft, kPageTop + 152, 120, 220);

    const int check_count = static_cast<int>(std::size(kAppearanceChecks));
    for (int index = 0; index < check_count; ++index) {
        const int column = index % 2;
        const int row = index / 2;
        HWND box = AddCheckBox(kPageAppearance, Loc(kAppearanceChecks[index].key),
                               kPageLeft + column * 170,
                               kPageTop + 196 + row * kRowStep, 160);
        if (box != nullptr) {
            bools_.push_back(BoolBinding{box, kAppearanceChecks[index].field});
        }
    }

    // ListFormat: legacy editable drop-down with the three presets the
    // original dialog offered (docs/spec/02-config-options.md §3.5).
    AddLabel(kPageAppearance, Loc("ConfigForm.LblListFormat"), kPageLeft + 360,
             kPageTop + 8, 280);
    HWND combo = MakeControl(kPageAppearance, WC_COMBOBOXW, L"",
                             WS_CHILD | WS_TABSTOP | CBS_DROPDOWN |
                                 CBS_AUTOHSCROLL | WS_VSCROLL,
                             kPageLeft + 360, kPageTop + 32, 300, 260, 0);
    if (combo != nullptr) {
        const wchar_t* const kPresets[] = {L"%-25s| %s", L"%s (%s)",
                                           L"%s [%s]"};
        for (const wchar_t* preset : kPresets) {
            ::SendMessageW(combo, CB_ADDSTRING, 0,
                           reinterpret_cast<LPARAM>(preset));
        }
        texts_.push_back(TextBinding{combo, &Settings::list_format});
        // The combo forwards focus to its internal edit child; subclass it too
        // so Tab/Esc work while the user is typing the format string.
        HWND combo_edit = ::GetWindow(combo, GW_CHILD);
        if (combo_edit != nullptr) {
            ::SetWindowSubclass(combo_edit, &ConfigWindow::ChildSubclassProc, 1,
                                reinterpret_cast<DWORD_PTR>(this));
        }
        ::SetWindowSubclass(combo, &ConfigWindow::ChildSubclassProc, 1,
                            reinterpret_cast<DWORD_PTR>(this));
    }

    AddLabel(kPageAppearance, Loc("resListFormatHint"), kPageLeft,
             kPageTop + 296, 660);
    AddLabel(kPageAppearance, Loc("resAppearanceHint"), kPageLeft,
             kPageTop + 320, 660);
}

void ConfigWindow::CreateAboutPage() {
    version_text_ = AddLabel(kPageAbout, L"", kPageLeft, kPageTop + 8, 660);
    ini_path_text_ = AddLabel(kPageAbout, L"", kPageLeft, kPageTop + 36, 660,
                              kRowHeight, SS_PATHELLIPSIS);
    run_count_text_ = AddLabel(kPageAbout, L"", kPageLeft, kPageTop + 64, 400);
    HWND trace = AddCheckBox(kPageAbout, Loc("resTraceEnable"), kPageLeft,
                             kPageTop + 100, 300);
    if (trace != nullptr) {
        bools_.push_back(BoolBinding{trace, &Settings::trace_enabled});
    }

    AddLabel(kPageAbout, Loc("resRelatedFiles"), kPageLeft,
             kPageTop + 148, 400);
    const std::wstring directory = ExeDirectory();
    const std::wstring param_history =
        Loc("resParamHistoryFile") + directory + L"ParamHistory.txt";
    const std::wstring shortcut_list =
        Loc("resShortcutListFile") + directory + L"ShortCutList.txt";
    const std::wstring favourite_list =
        Loc("resFavoriteListFile") + directory + L"FavoriteList.txt";
    AddLabel(kPageAbout, param_history.c_str(), kPageLeft, kPageTop + 176, 660,
             kRowHeight, SS_PATHELLIPSIS);
    AddLabel(kPageAbout, shortcut_list.c_str(), kPageLeft, kPageTop + 200, 660,
             kRowHeight, SS_PATHELLIPSIS);
    AddLabel(kPageAbout, favourite_list.c_str(), kPageLeft, kPageTop + 224, 660,
             kRowHeight, SS_PATHELLIPSIS);

    AddLabel(kPageAbout, Loc("resTraceEnableHint"), kPageLeft, kPageTop + 264,
             660);
}

void ConfigWindow::OnCreate() {
    dpi_ = static_cast<int>(::GetDpiForWindow(hwnd_));
    if (dpi_ <= 0) {
        dpi_ = USER_DEFAULT_SCREEN_DPI;
    }
    CreateFonts();
    background_brush_ = ::CreateSolidBrush(::GetSysColor(COLOR_BTNFACE));

    // The tab control sits at the bottom of the z-order; every page control is
    // a *sibling* above it (not a tab child) so switching pages only needs
    // ShowWindow and there is no theming artifact when controls disappear.
    // WS_CLIPSIBLINGS stops the tab from painting over those siblings.
    tab_ = MakeControl(-1, WC_TABCONTROLW, L"",
                       WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_TABSTOP,
                       kTabX, kTabY, kTabWidth, kTabHeight, 0);
    if (tab_ == nullptr) {
        Logger::Instance().Error("config window: cannot create tab control");
        return;
    }
    ::SetWindowSubclass(tab_, &ConfigWindow::ChildSubclassProc, 1,
                        reinterpret_cast<DWORD_PTR>(this));

    for (int index = 0; index < kPageCount; ++index) {
        const std::wstring label = Loc(kTabKeys[index]);
        TCITEMW item = {};
        item.mask = TCIF_TEXT;
        item.pszText = const_cast<LPWSTR>(label.c_str());
        TabCtrl_InsertItem(tab_, index, &item);
    }

    CreateControls();

    const int ok_x = kWindowWidth - kMargin - 2 * kButtonWidth - kButtonGap;
    ok_button_ = MakeControl(-1, WC_BUTTONW, Loc("Common.BtnOK"),
                             WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                             ok_x, kButtonY, kButtonWidth, kButtonHeight, IDOK);
    cancel_button_ = MakeControl(
        -1, WC_BUTTONW, Loc("Common.BtnCancel"),
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
        kWindowWidth - kMargin - kButtonWidth, kButtonY, kButtonWidth,
        kButtonHeight, IDCANCEL);
    if (ok_button_ != nullptr) {
        ::SetWindowSubclass(ok_button_, &ConfigWindow::ChildSubclassProc, 1,
                            reinterpret_cast<DWORD_PTR>(this));
    }
    if (cancel_button_ != nullptr) {
        ::SetWindowSubclass(cancel_button_, &ConfigWindow::ChildSubclassProc, 1,
                            reinterpret_cast<DWORD_PTR>(this));
    }

    SelectPage(kPageGeneral);
    Logger::Instance().Trace("config window created");
}

// ---------------------------------------------------------------------------
// Value flow
// ---------------------------------------------------------------------------

void ConfigWindow::LoadFromSettings() {
    for (const BoolBinding& binding : bools_) {
        ::SendMessageW(binding.hwnd, BM_SETCHECK,
                       (settings_.*(binding.field)) ? BST_CHECKED : BST_UNCHECKED,
                       0);
    }
    for (const IntBinding& binding : ints_) {
        SetControlText(binding.hwnd,
                       std::to_wstring(settings_.*(binding.field)));
    }
    for (const TextBinding& binding : texts_) {
        SetControlText(binding.hwnd, ToWide(settings_.*(binding.field)));
    }
    UpdateAboutTexts();
}

void ConfigWindow::SaveFromControls() {
    for (const BoolBinding& binding : bools_) {
        settings_.*(binding.field) =
            (::SendMessageW(binding.hwnd, BM_GETCHECK, 0, 0) == BST_CHECKED);
    }
    for (const IntBinding& binding : ints_) {
        const int current = settings_.*(binding.field);
        const int parsed = ReadIntControl(binding.hwnd, current);
        settings_.*(binding.field) =
            std::clamp(parsed, binding.min_value, binding.max_value);
    }
    for (const TextBinding& binding : texts_) {
        // The hotkey boxes already hold the legacy notation; trimming only
        // removes accidental surrounding whitespace typed into the text fields.
        const std::string utf8 = ToUtf8(GetControlText(binding.hwnd));
        settings_.*(binding.field) = std::string(TrimAscii(utf8));
    }
}

void ConfigWindow::UpdateAboutTexts() {
    SetControlText(version_text_, Loc("resLabelVersion") + kVersion);
    std::wstring ini_path = settings_.ini_path;
    if (ini_path.empty()) {
        ini_path = ExeDirectory() + L"ALTRun.ini";
    }
    SetControlText(ini_path_text_, Loc("resAboutIniPath") + ini_path);
    SetControlText(run_count_text_,
                   Loc("resAboutRunCount") +
                       std::to_wstring(settings_.shortcut_run_count));
}

void ConfigWindow::Confirm() {
    if (hwnd_ == nullptr || !visible()) {
        return;
    }
    SaveFromControls();
    if (!settings_.Save()) {
        Logger::Instance().Warn("config window: saving ALTRun.ini failed");
    }
    Logger::Instance().Info("config window: settings applied");
    if (apply_handler_) {
        apply_handler_();
    }
    Hide();
}

void ConfigWindow::Cancel() {
    if (hwnd_ == nullptr) {
        return;
    }
    if (visible()) {
        Hide();
    }
    // Throw away any half-edited control text so a later Open() starts from
    // Settings again.
    LoadFromSettings();
}

void ConfigWindow::Open() {
    if (hwnd_ == nullptr) {
        return;
    }
    LoadFromSettings();
    SelectPage(kPageGeneral);
    PlaceOnCursorMonitor();
    ::ShowWindow(hwnd_, SW_SHOWNORMAL);
    ::SetForegroundWindow(hwnd_);
    FocusFirstControl();
    ::InvalidateRect(hwnd_, nullptr, TRUE);
    Logger::Instance().Trace("config window opened");
}

// ---------------------------------------------------------------------------
// Tabs / focus
// ---------------------------------------------------------------------------

void ConfigWindow::SelectPage(int index) {
    if (index < 0 || index >= kPageCount) {
        index = kPageGeneral;
    }
    if (tab_ != nullptr && TabCtrl_GetCurSel(tab_) != index) {
        // TCM_SETCURSEL does not send TCN_SELCHANGE, so no recursion here.
        TabCtrl_SetCurSel(tab_, index);
    }
    current_page_ = index;
    for (int page = 0; page < kPageCount; ++page) {
        const bool show = page == index;
        for (HWND control : page_controls_[static_cast<size_t>(page)]) {
            ::ShowWindow(control, show ? SW_SHOW : SW_HIDE);
        }
    }
    if (hwnd_ != nullptr) {
        // Repaint the whole client: hidden controls leave holes in the tab
        // body and the native tab control does not repaint that area itself.
        ::InvalidateRect(hwnd_, nullptr, TRUE);
    }
}

void ConfigWindow::MoveFocus(bool backwards) {
    if (hwnd_ == nullptr) {
        return;
    }
    std::vector<HWND> order;
    for (HWND control : page_controls_[static_cast<size_t>(current_page_)]) {
        if (::IsWindowVisible(control) && IsFocusable(control)) {
            order.push_back(control);
        }
    }
    if (ok_button_ != nullptr && ::IsWindowVisible(ok_button_)) {
        order.push_back(ok_button_);
    }
    if (cancel_button_ != nullptr && ::IsWindowVisible(cancel_button_)) {
        order.push_back(cancel_button_);
    }
    if (order.empty()) {
        return;
    }

    HWND focused = ::GetFocus();
    // A combo box hands focus to its internal edit child; treat the pair as a
    // single tab stop.
    for (HWND candidate : order) {
        if (candidate == focused || ::IsChild(candidate, focused)) {
            focused = candidate;
            break;
        }
    }

    size_t position = order.size();
    for (size_t index = 0; index < order.size(); ++index) {
        if (order[index] == focused) {
            position = index;
            break;
        }
    }
    size_t next = 0;
    if (position == order.size()) {
        next = backwards ? order.size() - 1 : 0;
    } else if (backwards) {
        next = (position + order.size() - 1) % order.size();
    } else {
        next = (position + 1) % order.size();
    }
    ::SetFocus(order[next]);
}

void ConfigWindow::FocusFirstControl() {
    for (HWND control : page_controls_[static_cast<size_t>(current_page_)]) {
        if (::IsWindowVisible(control) && IsFocusable(control)) {
            ::SetFocus(control);
            return;
        }
    }
    if (ok_button_ != nullptr) {
        ::SetFocus(ok_button_);
    }
}

// ---------------------------------------------------------------------------
// Painting
// ---------------------------------------------------------------------------

void ConfigWindow::PlaceOnCursorMonitor() {
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

void ConfigWindow::DrawHeader(HDC dc) const {
    if (font_ == nullptr || title_font_ == nullptr) {
        return;
    }
    RECT client = {};
    ::GetClientRect(hwnd_, &client);

    ::SetBkMode(dc, TRANSPARENT);
    ::SetTextColor(dc, ::GetSysColor(COLOR_BTNTEXT));
    HGDIOBJ previous = ::SelectObject(dc, title_font_);
    RECT title = {S(20), S(4), client.right - S(20), S(kHeaderHeight - 6)};
    const std::wstring header =
        L"ALTRun " + Loc("ConfigForm.ConfigFormCaption");
    ::DrawTextW(dc, header.c_str(), -1, &title,
                DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
    ::SelectObject(dc, previous);

    const RECT separator = {S(16), S(kHeaderHeight), client.right - S(16),
                            S(kHeaderHeight) + 1};
    ::FillRect(dc, &separator, ::GetSysColorBrush(COLOR_3DSHADOW));
}

// ---------------------------------------------------------------------------
// Message handling
// ---------------------------------------------------------------------------

void ConfigWindow::PostButtonCommand(int id) const {
    if (hwnd_ == nullptr) {
        return;
    }
    ::PostMessageW(hwnd_, WM_COMMAND, MAKEWPARAM(id, BN_CLICKED), 0);
}

LRESULT ConfigWindow::HandleMessage(UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
        case WM_COMMAND: {
            if (HIWORD(wparam) == BN_CLICKED) {
                const int id = LOWORD(wparam);
                if (id == IDOK) {
                    Confirm();
                    return 0;
                }
                if (id == IDCANCEL) {
                    Cancel();
                    return 0;
                }
            }
            // Checkbox/COMBOBOX/EDIT notifications are intentionally ignored:
            // values are only harvested when 确定 is pressed.
            return 0;
        }
        case WM_NOTIFY: {
            const NMHDR* header = reinterpret_cast<const NMHDR*>(lparam);
            if (header != nullptr && header->hwndFrom == tab_ &&
                header->code == TCN_SELCHANGE) {
                SelectPage(TabCtrl_GetCurSel(tab_));
                return 0;
            }
            break;
        }
        case WM_CLOSE:
            // "Closing" the dialog means cancel, not DestroyWindow: the window
            // is created once and reused by App.
            Cancel();
            return 0;
        case WM_KEYDOWN:
            if (wparam == VK_ESCAPE) {
                Cancel();
                return 0;
            }
            if (wparam == VK_TAB) {
                MoveFocus((::GetKeyState(VK_SHIFT) & 0x8000) != 0);
                return 0;
            }
            if (wparam == VK_RETURN) {
                Confirm();
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
            // Static labels and the read-only hotkey boxes share the dialog
            // background instead of the default white.
            HDC dc = reinterpret_cast<HDC>(wparam);
            ::SetBkMode(dc, TRANSPARENT);
            return reinterpret_cast<LRESULT>(background_brush_);
        }
        case WM_DESTROY:
            // Also covered by the destructor; handled here for an explicit
            // Destroy() while the object is still fully alive.
            DestroyResources();
            return 0;
        default:
            break;
    }
    return Window::HandleMessage(message, wparam, lparam);
}

// ---------------------------------------------------------------------------
// Child control subclasses
// ---------------------------------------------------------------------------

LRESULT CALLBACK ConfigWindow::ChildSubclassProc(HWND hwnd, UINT message,
                                                 WPARAM wparam, LPARAM lparam,
                                                 UINT_PTR, DWORD_PTR ref_data) {
    auto* self = reinterpret_cast<ConfigWindow*>(ref_data);
    if (self == nullptr) {
        return ::DefSubclassProc(hwnd, message, wparam, lparam);
    }

    switch (message) {
        case WM_KEYDOWN: {
            const UINT vk = static_cast<UINT>(wparam);
            if (vk == VK_TAB) {
                self->MoveFocus((::GetKeyState(VK_SHIFT) & 0x8000) != 0);
                return 0;
            }
            if (vk == VK_ESCAPE) {
                ::PostMessageW(self->hwnd_, WM_CLOSE, 0, 0);
                return 0;
            }
            if (vk == VK_RETURN) {
                const bool on_cancel = hwnd == self->cancel_button_;
                self->PostButtonCommand(on_cancel ? IDCANCEL : IDOK);
                return 0;
            }
            break;
        }
        case WM_CHAR:
            // Swallow the characters for keys consumed above (no beeps).
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

LRESULT CALLBACK ConfigWindow::HotkeySubclassProc(HWND hwnd, UINT message,
                                                  WPARAM wparam, LPARAM lparam,
                                                  UINT_PTR, DWORD_PTR ref_data) {
    auto* self = reinterpret_cast<ConfigWindow*>(ref_data);
    if (self == nullptr) {
        return ::DefSubclassProc(hwnd, message, wparam, lparam);
    }

    switch (message) {
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN: {
            const UINT vk = static_cast<UINT>(wparam);
            if (vk == VK_ESCAPE) {
                ::PostMessageW(self->hwnd_, WM_CLOSE, 0, 0);
                return 0;
            }
            if (vk == VK_RETURN) {
                self->PostButtonCommand(IDOK);
                return 0;
            }

            const bool ctrl = (::GetKeyState(VK_CONTROL) & 0x8000) != 0;
            const bool alt = (::GetKeyState(VK_MENU) & 0x8000) != 0;
            const bool shift = (::GetKeyState(VK_SHIFT) & 0x8000) != 0;
            const bool win = ((::GetKeyState(VK_LWIN) & 0x8000) != 0) ||
                             ((::GetKeyState(VK_RWIN) & 0x8000) != 0);

            // Plain Tab keeps its dialog navigation meaning; Ctrl+Tab (or any
            // other modified Tab) is captured as a hotkey.
            if (vk == VK_TAB && !ctrl && !alt && !shift && !win) {
                self->MoveFocus(false);
                return 0;
            }
            if (IsModifierKey(vk)) {
                // Wait for a real key; the modifiers are read with GetKeyState.
                return 0;
            }
            if (vk == VK_DELETE && !ctrl && !alt && !shift && !win) {
                ::SetWindowTextW(hwnd, L"");  // HotKey2 empty == disabled
                return 0;
            }

            const std::wstring text = FormatHotkey(vk, ctrl, alt, shift, win);
            if (text.empty()) {
                // Keys outside the legacy ini notation (media keys, OEM keys
                // other than `) are ignored instead of writing unparsable text.
                return 0;
            }
            ::SetWindowTextW(hwnd, text.c_str());
            return 0;
        }
        case WM_CHAR:
        case WM_SYSCHAR:
        case WM_KEYUP:
        case WM_SYSKEYUP:
            return 0;
        default:
            break;
    }
    return ::DefSubclassProc(hwnd, message, wparam, lparam);
}

}  // namespace alt
