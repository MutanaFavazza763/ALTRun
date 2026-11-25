#include "ui/ShortcutEditWindow.h"

#include <commctrl.h>
#include <dwmapi.h>
#include <objbase.h>
#include <shobjidl.h>
#include <windowsx.h>

#include <iterator>
#include <string>
#include <utility>
#include <vector>

#include "core/Launcher.h"
#include "core/Logger.h"
#include "core/ShortcutSource.h"
#include "core/Text.h"
#include "ui/FluentTheme.h"
#include "ui/UiText.h"

namespace alt {
namespace {

constexpr wchar_t kWindowClass[] = L"ALTRun.ShortcutEditWindow.v3";

// Layout in 96-DPI design units (scaled with ShortcutEditWindow::S()).
constexpr int kWindowWidth = 600;
constexpr int kWindowHeight = 520;
constexpr int kHeaderHeight = 48;
constexpr int kMargin = 18;
constexpr int kLabelWidth = 100;
constexpr int kFieldX = 140;
constexpr int kFieldWidth = 440;
constexpr int kRowHeight = 28;
constexpr int kButtonHeight = 34;

constexpr int kIdKeyword = 1001;
constexpr int kIdName = 1002;
constexpr int kIdCommandLine = 1003;
constexpr int kIdParamNone = 1010;
constexpr int kIdParamNoEncoding = 1011;
constexpr int kIdParamUrlQuery = 1012;
constexpr int kIdParamUtf8Query = 1013;
constexpr int kIdPickFile = 1020;
constexpr int kIdPickFolder = 1021;
constexpr int kIdTest = 1022;

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

std::wstring FilesystemPath(IShellItem* item) {
    if (item == nullptr) {
        return {};
    }
    PWSTR path = nullptr;
    std::wstring result;
    if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path)) && path != nullptr) {
        result = path;
        ::CoTaskMemFree(path);
    }
    return result;
}

// IFileDialog based pickers (COM is initialized by wWinMain; a failed COM call
// is reported as "cancelled" instead of crashing).
std::wstring PickFileNative(HWND owner, const wchar_t* title) {
    IFileOpenDialog* dialog = nullptr;
    if (FAILED(::CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
                                  IID_PPV_ARGS(&dialog))) ||
        dialog == nullptr) {
        return {};
    }
    DWORD options = 0;
    if (SUCCEEDED(dialog->GetOptions(&options))) {
        (void)dialog->SetOptions(options | FOS_FORCEFILESYSTEM | FOS_FILEMUSTEXIST |
                                 FOS_PATHMUSTEXIST | FOS_NOCHANGEDIR);
    }
    if (title != nullptr) {
        (void)dialog->SetTitle(title);
    }
    std::wstring result;
    if (SUCCEEDED(dialog->Show(owner))) {
        IShellItem* item = nullptr;
        if (SUCCEEDED(dialog->GetResult(&item)) && item != nullptr) {
            result = FilesystemPath(item);
            item->Release();
        }
    }
    dialog->Release();
    return result;
}

std::wstring PickFolderNative(HWND owner, const wchar_t* title) {
    IFileOpenDialog* dialog = nullptr;
    if (FAILED(::CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
                                  IID_PPV_ARGS(&dialog))) ||
        dialog == nullptr) {
        return {};
    }
    DWORD options = 0;
    if (SUCCEEDED(dialog->GetOptions(&options))) {
        (void)dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM |
                                 FOS_PATHMUSTEXIST | FOS_NOCHANGEDIR);
    }
    if (title != nullptr) {
        (void)dialog->SetTitle(title);
    }
    std::wstring result;
    if (SUCCEEDED(dialog->Show(owner))) {
        IShellItem* item = nullptr;
        if (SUCCEEDED(dialog->GetResult(&item)) && item != nullptr) {
            result = FilesystemPath(item);
            item->Release();
        }
    }
    dialog->Release();
    return result;
}

}  // namespace

ShortcutEditWindow::ShortcutEditWindow() = default;

// GDI objects are released here (not in WM_DESTROY) because the window object is
// destroyed by ~Window(), when the dynamic type has already degraded to Window
// and the message handler of this class no longer runs. Same pattern as
// ConfigWindow.
ShortcutEditWindow::~ShortcutEditWindow() { DestroyResources(); }

bool ShortcutEditWindow::CreateEditWindow(HINSTANCE instance) {
    // WS_CLIPCHILDREN keeps the native controls from being painted over by the
    // parent background; WS_EX_TOPMOST/WS_EX_TOOLWINDOW match the other ALTRun
    // windows (no taskbar entry).
    if (!Create(instance, kWindowClass,
                Loc("ShortCutForm.ShortCutFormCaption"), WS_POPUP,
                WS_EX_TOPMOST | WS_EX_TOOLWINDOW)) {
        return false;
    }
    const DWORD corner_preference = 2;  // DWMWCP_ROUND (Windows 11), best effort
    ::DwmSetWindowAttribute(hwnd_, 33, &corner_preference,
                            sizeof(corner_preference));
    return true;
}

int ShortcutEditWindow::S(int value) const {
    return ::MulDiv(value, dpi_, USER_DEFAULT_SCREEN_DPI);
}

void ShortcutEditWindow::CreateFonts() {
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
        title_log.lfHeight = -::MulDiv(12, dpi_, 72);
        title_log.lfWeight = FW_SEMIBOLD;
        title_font_ = ::CreateFontIndirectW(&title_log);
    }
}

void ShortcutEditWindow::DestroyResources() {
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

void ShortcutEditWindow::ApplyFont(HWND control) const {
    if (control != nullptr && font_ != nullptr) {
        ::SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font_), TRUE);
    }
}

HWND ShortcutEditWindow::MakeControl(const wchar_t* class_name,
                                     const std::wstring& text, DWORD style, int x,
                                     int y, int width, int height, int id) {
    HWND control = ::CreateWindowExW(
        0, class_name, text.c_str(), style, S(x), S(y), S(width), S(height),
        hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), instance_,
        nullptr);
    if (control == nullptr) {
        Logger::Instance().Warn(std::string("shortcut edit window: cannot create ") +
                                ToUtf8(class_name));
        return nullptr;
    }
    ApplyFont(control);
    return control;
}

void ShortcutEditWindow::CreateControls() {
    MakeControl(WC_STATICW, Loc("resLabelKeyword"),
                WS_CHILD | WS_VISIBLE | SS_CENTERIMAGE, kMargin + 18, 92,
                kLabelWidth, kRowHeight, 0);
    keyword_edit_ = MakeControl(WC_EDITW, L"",
                                WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER |
                                    ES_LEFT | ES_AUTOHSCROLL,
                                kFieldX, 92, kFieldWidth, kRowHeight, kIdKeyword);

    MakeControl(WC_STATICW, Loc("resLabelName"),
                WS_CHILD | WS_VISIBLE | SS_CENTERIMAGE, kMargin + 18, 128,
                kLabelWidth, kRowHeight, 0);
    name_edit_ = MakeControl(WC_EDITW, L"",
                             WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER |
                                 ES_LEFT | ES_AUTOHSCROLL,
                             kFieldX, 128, kFieldWidth, kRowHeight, kIdName);

    MakeControl(WC_STATICW, Loc("resLabelCommandLine"),
                WS_CHILD | WS_VISIBLE | SS_CENTERIMAGE, kMargin + 18, 164,
                kLabelWidth, kRowHeight, 0);
    command_line_edit_ =
        MakeControl(WC_EDITW, L"",
                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER | ES_LEFT |
                        ES_AUTOHSCROLL,
                    kFieldX, 164, kFieldWidth, kRowHeight, kIdCommandLine);

    MakeControl(WC_STATICW, Loc("resLabelParamType"),
                WS_CHILD | WS_VISIBLE | SS_CENTERIMAGE, kMargin + 18, 206,
                kLabelWidth, kRowHeight, 0);
    radio_none_ = fluent::CreateRadio(hwnd_, instance_, kIdParamNone,
                                      Loc("resParamNone"), font_, 160,
                                      kRowHeight, dpi_);
    radio_no_encoding_ = fluent::CreateRadio(
        hwnd_, instance_, kIdParamNoEncoding, Loc("resParamNoEncoding"), font_,
        170, kRowHeight, dpi_);
    radio_url_query_ = fluent::CreateRadio(hwnd_, instance_, kIdParamUrlQuery,
                                           Loc("resParamUrlQuery"), font_, 160,
                                           kRowHeight, dpi_);
    radio_utf8_query_ = fluent::CreateRadio(
        hwnd_, instance_, kIdParamUtf8Query, Loc("resParamUtf8Query"), font_,
        170, kRowHeight, dpi_);
    if (radio_none_ != nullptr) {
        ::SetWindowPos(radio_none_, nullptr, S(kFieldX), S(206), 0, 0,
                       SWP_NOSIZE | SWP_NOZORDER);
    }
    if (radio_no_encoding_ != nullptr) {
        ::SetWindowPos(radio_no_encoding_, nullptr, S(kFieldX + 164), S(206),
                       0, 0, SWP_NOSIZE | SWP_NOZORDER);
    }
    if (radio_url_query_ != nullptr) {
        ::SetWindowPos(radio_url_query_, nullptr, S(kFieldX), S(238), 0, 0,
                       SWP_NOSIZE | SWP_NOZORDER);
    }
    if (radio_utf8_query_ != nullptr) {
        ::SetWindowPos(radio_utf8_query_, nullptr, S(kFieldX + 164), S(238), 0,
                       0, SWP_NOSIZE | SWP_NOZORDER);
    }

    auto make_button = [this](int id, const std::wstring& text, int x, int y,
                              int width, bool primary) {
        HWND button = fluent::CreateButton(hwnd_, instance_, id, text, font_,
                                           width, kButtonHeight, dpi_, primary);
        if (button != nullptr) {
            ::SetWindowPos(button, nullptr, S(x), S(y), 0, 0,
                           SWP_NOSIZE | SWP_NOZORDER);
            ::SetWindowSubclass(button, &ShortcutEditWindow::ChildSubclassProc,
                                2, reinterpret_cast<DWORD_PTR>(this));
        }
        return button;
    };

    pick_file_button_ = make_button(kIdPickFile, Loc("resPickFile"), kFieldX,
                                    278, 120, false);
    pick_folder_button_ =
        make_button(kIdPickFolder, Loc("resPickFolder"), kFieldX + 128, 278, 120,
                    false);
    test_button_ =
        make_button(kIdTest, Loc("Common.BtnTest"), kFieldX + 256, 278, 90,
                    false);

    hint_label_ = MakeControl(
        WC_STATICW, Loc("resEditHint"), WS_CHILD | WS_VISIBLE | SS_LEFT,
        kFieldX, 322, kFieldWidth, 58, 0);

    ok_button_ = make_button(IDOK, Loc("Common.BtnOK"),
                             kWindowWidth - kMargin - 2 * 96 - 8, 452, 96, true);
    cancel_button_ = make_button(IDCANCEL, Loc("Common.BtnCancel"),
                                 kWindowWidth - kMargin - 96, 452, 96, false);

    HWND subclassed[] = {keyword_edit_,      name_edit_,       command_line_edit_,
                         radio_none_,        radio_no_encoding_, radio_url_query_,
                         radio_utf8_query_};
    for (HWND control : subclassed) {
        if (control != nullptr) {
            ::SetWindowSubclass(control, &ShortcutEditWindow::ChildSubclassProc, 2,
                                reinterpret_cast<DWORD_PTR>(this));
        }
    }
}

void ShortcutEditWindow::OnCreate() {
    dpi_ = static_cast<int>(::GetDpiForWindow(hwnd_));
    if (dpi_ <= 0) {
        dpi_ = USER_DEFAULT_SCREEN_DPI;
    }
    CreateFonts();
    background_brush_ = ::CreateSolidBrush(::GetSysColor(COLOR_BTNFACE));
    CreateControls();
}

void ShortcutEditWindow::LoadFromShortcut(const Shortcut& item, bool is_new) {
    SetControlText(keyword_edit_, ToWide(item.keyword));
    SetControlText(name_edit_, ToWide(item.name));
    SetControlText(command_line_edit_, ToWide(item.command_line));

    ParamType type = is_new ? ParamType::None : item.param_type;
    ::SendMessageW(radio_none_, BM_SETCHECK,
                   type == ParamType::None ? BST_CHECKED : BST_UNCHECKED, 0);
    ::SendMessageW(radio_no_encoding_, BM_SETCHECK,
                   type == ParamType::NoEncoding ? BST_CHECKED : BST_UNCHECKED, 0);
    ::SendMessageW(radio_url_query_, BM_SETCHECK,
                   type == ParamType::UrlQuery ? BST_CHECKED : BST_UNCHECKED, 0);
    ::SendMessageW(radio_utf8_query_, BM_SETCHECK,
                   type == ParamType::Utf8Query ? BST_CHECKED : BST_UNCHECKED, 0);
}

std::string ShortcutEditWindow::ReadTrimmed(HWND control) const {
    if (control == nullptr) {
        return {};
    }
    const std::string utf8 = ToUtf8(GetControlText(control));
    return std::string(TrimAscii(utf8));
}

Shortcut ShortcutEditWindow::CurrentShortcut() const {
    Shortcut item;
    item.keyword = ReadTrimmed(keyword_edit_);
    item.name = ReadTrimmed(name_edit_);
    item.command_line = ReadTrimmed(command_line_edit_);
    item.freq = editing_new_ ? 0 : original_.freq;
    item.param_type = ParamType::None;
    if (::SendMessageW(radio_no_encoding_, BM_GETCHECK, 0, 0) == BST_CHECKED) {
        item.param_type = ParamType::NoEncoding;
    } else if (::SendMessageW(radio_url_query_, BM_GETCHECK, 0, 0) == BST_CHECKED) {
        item.param_type = ParamType::UrlQuery;
    } else if (::SendMessageW(radio_utf8_query_, BM_GETCHECK, 0, 0) == BST_CHECKED) {
        item.param_type = ParamType::Utf8Query;
    }
    return item;
}

void ShortcutEditWindow::Open(const Shortcut* item, ConfirmHandler on_confirm) {
    if (hwnd_ == nullptr || modal_) {
        return;  // nested Open() while already shown: ignored
    }
    on_confirm_ = std::move(on_confirm);
    editing_new_ = item == nullptr;
    original_ = item != nullptr ? *item : Shortcut{};
    LoadFromShortcut(original_, editing_new_);
    // The legacy caption was just "快捷项"; the rewrite tells add and edit apart.
    const std::wstring title =
        Loc(editing_new_ ? "resTitleAddShortcut" : "resTitleEditShortcut");
    ::SetWindowTextW(hwnd_, title.c_str());

    owner_ = ::GetActiveWindow();
    if (owner_ == hwnd_) {
        owner_ = nullptr;
    }
    if (owner_ == nullptr) {
        owner_ = ::GetWindow(hwnd_, GW_OWNER);
    }
    if (owner_ != nullptr) {
        ::SetWindowLongPtrW(hwnd_, GWLP_HWNDPARENT,
                            reinterpret_cast<LONG_PTR>(owner_));
        ::EnableWindow(owner_, FALSE);
    }
    PlaceOnOwner();
    modal_ = true;
    ::ShowWindow(hwnd_, SW_SHOWNORMAL);
    ::SetForegroundWindow(hwnd_);
    FocusFirstControl();

    // Nested message loop: the dialog behaves like a classic modal dialog while
    // the rest of the application keeps pumping messages.
    MSG message = {};
    bool quit = false;
    while (modal_) {
        const BOOL received = ::GetMessageW(&message, nullptr, 0, 0);
        if (received == 0) {
            quit = true;
            break;
        }
        if (received < 0) {
            break;
        }
        ::TranslateMessage(&message);
        ::DispatchMessageW(&message);
    }

    modal_ = false;
    on_confirm_ = nullptr;
    if (owner_ != nullptr) {
        ::EnableWindow(owner_, TRUE);
        ::SetForegroundWindow(owner_);
        owner_ = nullptr;
    }
    if (quit) {
        ::PostQuitMessage(static_cast<int>(message.wParam));
    }
}

void ShortcutEditWindow::Confirm() {
    if (!modal_) {
        return;
    }
    const std::string keyword = ReadTrimmed(keyword_edit_);
    const std::string name = ReadTrimmed(name_edit_);
    const std::string command_line = ReadTrimmed(command_line_edit_);

    // Both text fields empty: the user wants a blank separator row.
    if (keyword.empty() && command_line.empty()) {
        Finish(Shortcut{}, true);
        return;
    }
    if (keyword.empty()) {
        ::MessageBoxW(hwnd_, Loc("resKeywordEmpty").c_str(),
                      Loc("ShortCutForm.ShortCutFormCaption").c_str(),
                      MB_OK | MB_ICONWARNING);
        ::SetFocus(keyword_edit_);
        return;
    }
    if (keyword.find('|') != std::string::npos) {
        MessageBoxW(hwnd_, Loc("resKeywordPipe").c_str(),
                    Loc("ShortCutForm.ShortCutFormCaption").c_str(),
                    MB_OK | MB_ICONWARNING);
        ::SetFocus(keyword_edit_);
        return;
    }
    if (name.find(',') != std::string::npos ||
        name.find('|') != std::string::npos) {
        MessageBoxW(hwnd_, Loc("resNameSeparator").c_str(),
                    Loc("ShortCutForm.ShortCutFormCaption").c_str(),
                    MB_OK | MB_ICONWARNING);
        ::SetFocus(name_edit_);
        return;
    }

    Shortcut item = CurrentShortcut();
    if (item.name.empty() && editing_new_) {
        item.name = item.keyword;  // suggestion only, never required
    }
    Finish(item, false);
}

void ShortcutEditWindow::Finish(const Shortcut& item, bool is_blank) {
    modal_ = false;
    if (hwnd_ != nullptr) {
        ::ShowWindow(hwnd_, SW_HIDE);
    }
    // Called after hiding: the manager may show a "duplicate item" message box,
    // which must not be covered by this topmost window.
    if (on_confirm_) {
        on_confirm_(item, is_blank);
    }
}

void ShortcutEditWindow::Cancel() {
    if (!modal_) {
        return;
    }
    modal_ = false;
    if (hwnd_ != nullptr) {
        ::ShowWindow(hwnd_, SW_HIDE);
    }
}

void ShortcutEditWindow::TestCommandLine() {
    const Shortcut item = CurrentShortcut();
    if (item.command_line.empty()) {
        MessageBoxW(hwnd_, Loc("resNeedCommandLine").c_str(),
                    Loc("Common.BtnTest").c_str(), MB_OK | MB_ICONWARNING);
        return;
    }
    const LaunchResult result = LaunchShortcut(item, "");
    if (result.ok) {
        return;
    }
    std::wstring text = Loc("resTestFailed");
    text += ToWide(item.command_line);
    if (!result.message.empty()) {
        text += L"\r\n";
        text += ToWide(result.message);
    }
    if (result.error != 0) {
        text += Loc("resTestErrorCode");
        text += std::to_wstring(result.error);
    }
    MessageBoxW(hwnd_, text.c_str(), Loc("Common.BtnTest").c_str(),
                MB_OK | MB_ICONWARNING);
}

void ShortcutEditWindow::PickFile() {
    const std::wstring path = PickFileNative(hwnd_, Loc("resPickFileTitle").c_str());
    if (!path.empty()) {
        ApplyPickedPath(path);
    }
}

void ShortcutEditWindow::PickFolder() {
    const std::wstring path =
        PickFolderNative(hwnd_, Loc("ShortCutForm.SelectDir").c_str());
    if (!path.empty()) {
        ApplyPickedPath(path);
    }
}

void ShortcutEditWindow::ApplyPickedPath(const std::wstring& path) {
    // ShortcutFromPath resolves .lnk targets and derives keyword/name with the
    // same rules the legacy "add by file" path used.
    const std::string utf8_path = ToUtf8(path);
    const Shortcut derived = ShortcutFromPath(utf8_path);
    const std::string command_line =
        derived.command_line.empty() ? utf8_path : derived.command_line;
    SetControlText(command_line_edit_, ToWide(command_line));
    if (ReadTrimmed(keyword_edit_).empty() && !derived.keyword.empty()) {
        SetControlText(keyword_edit_, ToWide(derived.keyword));
    }
    if (ReadTrimmed(name_edit_).empty() && !derived.name.empty()) {
        SetControlText(name_edit_, ToWide(derived.name));
    }
}

void ShortcutEditWindow::MoveFocus(bool backwards) {
    if (hwnd_ == nullptr) {
        return;
    }
    const HWND order[] = {keyword_edit_,
                          name_edit_,
                          command_line_edit_,
                          radio_none_,
                          pick_file_button_,
                          pick_folder_button_,
                          test_button_,
                          ok_button_,
                          cancel_button_};
    std::vector<HWND> candidates;
    for (HWND control : order) {
        if (control == nullptr || ::IsWindowVisible(control) == FALSE) {
            continue;
        }
        if ((::GetWindowLongPtrW(control, GWL_STYLE) & WS_TABSTOP) == 0) {
            continue;
        }
        candidates.push_back(control);
    }
    if (candidates.empty()) {
        return;
    }

    HWND focused = ::GetFocus();
    // Any radio of the group is represented by the group leader.
    if (focused == radio_no_encoding_ || focused == radio_url_query_ ||
        focused == radio_utf8_query_) {
        focused = radio_none_;
    }
    size_t position = candidates.size();
    for (size_t index = 0; index < candidates.size(); ++index) {
        if (candidates[index] == focused) {
            position = index;
            break;
        }
    }
    size_t next = 0;
    if (position == candidates.size()) {
        next = backwards ? candidates.size() - 1 : 0;
    } else if (backwards) {
        next = (position + candidates.size() - 1) % candidates.size();
    } else {
        next = (position + 1) % candidates.size();
    }
    ::SetFocus(candidates[next]);
}

void ShortcutEditWindow::FocusFirstControl() {
    if (keyword_edit_ != nullptr) {
        ::SetFocus(keyword_edit_);
        ::SendMessageW(keyword_edit_, EM_SETSEL, 0, -1);
    } else if (ok_button_ != nullptr) {
        ::SetFocus(ok_button_);
    }
}

void ShortcutEditWindow::PlaceOnOwner() {
    if (hwnd_ == nullptr) {
        return;
    }
    RECT area = {};
    bool have_area = false;
    if (owner_ != nullptr && ::GetWindowRect(owner_, &area) != FALSE &&
        area.right > area.left && area.bottom > area.top) {
        have_area = true;
    }
    if (!have_area) {
        POINT cursor = {};
        ::GetCursorPos(&cursor);
        HMONITOR monitor = ::MonitorFromPoint(cursor, MONITOR_DEFAULTTONEAREST);
        MONITORINFO info = {};
        info.cbSize = static_cast<DWORD>(sizeof(info));
        if (::GetMonitorInfoW(monitor, &info) != FALSE) {
            area = info.rcWork;
            have_area = true;
        }
    }
    const int width = S(kWindowWidth);
    const int height = S(kWindowHeight);
    int x = 0;
    int y = 0;
    if (have_area) {
        x = area.left + ((area.right - area.left) - width) / 2;
        y = area.top + ((area.bottom - area.top) - height) / 2;
    }
    ::SetWindowPos(hwnd_, HWND_TOPMOST, x, y, width, height, SWP_NOACTIVATE);
}

void ShortcutEditWindow::DrawHeader(HDC dc) const {
    if (font_ == nullptr || title_font_ == nullptr) {
        return;
    }
    RECT client = {};
    ::GetClientRect(hwnd_, &client);
    wchar_t title[128] = {};
    ::GetWindowTextW(hwnd_, title, static_cast<int>(std::size(title)));
    fluent::FillBackground(dc, client);
    fluent::DrawTitleBar(dc, hwnd_, title, title_font_, S(kHeaderHeight));
    fluent::DrawCard(dc,
                     RECT{S(kMargin), S(64), S(kWindowWidth - kMargin), S(504)},
                     S(14));
}

void ShortcutEditWindow::OnPaint() {
    PAINTSTRUCT paint = {};
    HDC dc = ::BeginPaint(hwnd_, &paint);
    DrawHeader(dc);
    ::EndPaint(hwnd_, &paint);
}

void ShortcutEditWindow::PostButtonCommand(int id) const {
    if (hwnd_ == nullptr) {
        return;
    }
    ::PostMessageW(hwnd_, WM_COMMAND, MAKEWPARAM(id, BN_CLICKED), 0);
}

LRESULT ShortcutEditWindow::HandleMessage(UINT message, WPARAM wparam,
                                          LPARAM lparam) {
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
        case WM_COMMAND: {
            if (HIWORD(wparam) == BN_CLICKED) {
                const int id = LOWORD(wparam);
                if (id >= kIdParamNone && id <= kIdParamUtf8Query) {
                    ::CheckRadioButton(hwnd_, kIdParamNone, kIdParamUtf8Query,
                                       id);
                    return 0;
                }
                switch (id) {
                    case IDOK:
                        Confirm();
                        return 0;
                    case IDCANCEL:
                        Cancel();
                        return 0;
                    case kIdPickFile:
                        PickFile();
                        return 0;
                    case kIdPickFolder:
                        PickFolder();
                        return 0;
                    case kIdTest:
                        TestCommandLine();
                        return 0;
                    default:
                        break;
                }
            }
            return 0;  // radio/EDIT notifications need no action
        }
        case WM_CLOSE:
            Cancel();
            return 0;
        case WM_KEYDOWN:
            if (wparam == VK_ESCAPE) {
                Cancel();
                return 0;
            }
            if (wparam == VK_RETURN) {
                Confirm();
                return 0;
            }
            if (wparam == VK_TAB) {
                MoveFocus((::GetKeyState(VK_SHIFT) & 0x8000) != 0);
                return 0;
            }
            break;
        case WM_ERASEBKGND:
            return 1;
        case WM_CTLCOLORSTATIC: {
            HDC dc = reinterpret_cast<HDC>(wparam);
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
            // Also covered by the destructor; kept for an explicit Destroy().
            DestroyResources();
            return 0;
        default:
            break;
    }
    return Window::HandleMessage(message, wparam, lparam);
}

LRESULT CALLBACK ShortcutEditWindow::ChildSubclassProc(HWND hwnd, UINT message,
                                                       WPARAM wparam, LPARAM lparam,
                                                       UINT_PTR, DWORD_PTR ref_data) {
    auto* self = reinterpret_cast<ShortcutEditWindow*>(ref_data);
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
            // Swallow the characters of the keys consumed above (no beeps).
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
