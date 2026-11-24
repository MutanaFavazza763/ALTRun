#include "ui/ShortcutManagerWindow.h"

#include <commctrl.h>
#include <dwmapi.h>
#include <objbase.h>
#include <shobjidl.h>
#include <windowsx.h>

#include <algorithm>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

#include "core/Encoding.h"
#include "core/Logger.h"
#include "core/Text.h"
#include "ui/ShortcutEditWindow.h"
#include "ui/UiText.h"

namespace alt {
namespace {

constexpr wchar_t kWindowClass[] = L"ALTRun.ShortcutManagerWindow.v2";

// Layout in 96-DPI design units (scaled with ShortcutManagerWindow::S()).
constexpr int kWindowWidth = 900;
constexpr int kWindowHeight = 560;
constexpr int kHeaderHeight = 44;
constexpr int kListX = 10;
constexpr int kListY = 52;
constexpr int kListWidth = 880;
constexpr int kListHeight = 400;
constexpr int kRowButtonWidth = 84;
constexpr int kRowButtonHeight = 30;
constexpr int kRowButtonY = 462;
constexpr int kRowButtonGap = 8;
constexpr int kSmallButtonWidth = 76;
constexpr int kSmallButtonHeight = 28;
constexpr int kSmallButtonY = 502;

constexpr int kIdList = 1000;
constexpr int kIdAdd = 2001;
constexpr int kIdEdit = 2002;
constexpr int kIdDelete = 2003;
constexpr int kIdUp = 2004;
constexpr int kIdDown = 2005;
constexpr int kIdOk = 2006;
constexpr int kIdExport = 2007;
constexpr int kIdImport = 2008;

constexpr size_t kMaxImportFileSize = 32u * 1024u * 1024u;

// Column headers reuse the legacy [Common] keys; "关键字" and "显示名" have no
// legacy counterpart, so they keep rewrite keys.
const char* const kColumnKeys[] = {"resKeyword", "Common.Name", "Common.ParamType",
                                   "Common.CommandLine"};
constexpr int kColumnWidths[] = {140, 140, 100, 480};

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

std::wstring PickOpenFileNative(HWND owner, const wchar_t* title) {
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

std::wstring PickSaveFileNative(HWND owner, const wchar_t* title,
                                const wchar_t* default_name) {
    IFileSaveDialog* dialog = nullptr;
    if (FAILED(::CoCreateInstance(CLSID_FileSaveDialog, nullptr, CLSCTX_INPROC_SERVER,
                                  IID_PPV_ARGS(&dialog))) ||
        dialog == nullptr) {
        return {};
    }
    DWORD options = 0;
    if (SUCCEEDED(dialog->GetOptions(&options))) {
        (void)dialog->SetOptions(options | FOS_FORCEFILESYSTEM | FOS_OVERWRITEPROMPT |
                                 FOS_PATHMUSTEXIST | FOS_NOCHANGEDIR);
    }
    if (title != nullptr) {
        (void)dialog->SetTitle(title);
    }
    if (default_name != nullptr) {
        (void)dialog->SetFileName(default_name);
    }
    (void)dialog->SetDefaultExtension(L"txt");
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

size_t CountItems(const std::vector<ShortcutEntry>& entries) {
    return static_cast<size_t>(std::count_if(
        entries.begin(), entries.end(),
        [](const ShortcutEntry& entry) { return !entry.is_blank; }));
}

}  // namespace

ShortcutManagerWindow::ShortcutManagerWindow(Settings& settings, ShortcutStore& store)
    : settings_(settings), store_(store) {}

// The editor window is owned by unique_ptr and destroyed before ~Window();
// fonts/brushes are released here as well because the dynamic type has already
// degraded to Window when the base destructor destroys the HWND (same pattern
// as ConfigWindow).
ShortcutManagerWindow::~ShortcutManagerWindow() {
    edit_window_.reset();
    DestroyResources();
}

bool ShortcutManagerWindow::CreateManagerWindow(HINSTANCE instance) {
    INITCOMMONCONTROLSEX controls = {};
    controls.dwSize = static_cast<DWORD>(sizeof(controls));
    controls.dwICC = ICC_LISTVIEW_CLASSES;
    ::InitCommonControlsEx(&controls);

    if (!Create(instance, kWindowClass,
                Loc("ShortCutManForm.ShortCutManFormCaption"),
                WS_POPUP | WS_CLIPCHILDREN,
                WS_EX_TOPMOST | WS_EX_TOOLWINDOW)) {
        return false;
    }
    const DWORD corner_preference = 2;  // DWMWCP_ROUND (Windows 11), best effort
    ::DwmSetWindowAttribute(hwnd_, 33, &corner_preference,
                            sizeof(corner_preference));

    // One reusable modal editor shared by 新增 and 编辑.
    edit_window_ = std::make_unique<ShortcutEditWindow>();
    if (!edit_window_->CreateEditWindow(instance)) {
        Logger::Instance().Warn("cannot create shortcut edit window");
        edit_window_.reset();
    }

    PlaceOnCursorMonitor();
    return true;
}

void ShortcutManagerWindow::SetChangedHandler(std::function<void()> handler) {
    changed_handler_ = std::move(handler);
}

int ShortcutManagerWindow::S(int value) const {
    return ::MulDiv(value, dpi_, USER_DEFAULT_SCREEN_DPI);
}

void ShortcutManagerWindow::CreateFonts() {
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
        title_log.lfHeight = static_cast<LONG>(title_log.lfHeight * 1.3);
        title_log.lfWeight = FW_SEMIBOLD;
        title_font_ = ::CreateFontIndirectW(&title_log);
    }
}

void ShortcutManagerWindow::DestroyResources() {
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

void ShortcutManagerWindow::ApplyFont(HWND control) const {
    if (control != nullptr && font_ != nullptr) {
        ::SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font_), TRUE);
    }
}

HWND ShortcutManagerWindow::MakeControl(const wchar_t* class_name,
                                        const std::wstring& text, DWORD style,
                                        int x, int y, int width, int height,
                                        int id) {
    HWND control = ::CreateWindowExW(
        0, class_name, text.c_str(), style, S(x), S(y), S(width), S(height),
        hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), instance_,
        nullptr);
    if (control == nullptr) {
        Logger::Instance().Warn(std::string("shortcut manager: cannot create ") +
                                ToUtf8(class_name));
        return nullptr;
    }
    ApplyFont(control);
    return control;
}

void ShortcutManagerWindow::CreateControls() {
    list_ = MakeControl(WC_LISTVIEWW, L"",
                        WS_CHILD | WS_VISIBLE | WS_TABSTOP | LVS_REPORT |
                            LVS_SINGLESEL | LVS_SHOWSELALWAYS,
                        kListX, kListY, kListWidth, kListHeight, kIdList);
    if (list_ != nullptr) {
        ListView_SetExtendedListViewStyle(
            list_, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_LABELTIP);
        const int column_count = static_cast<int>(std::size(kColumnKeys));
        for (int index = 0; index < column_count; ++index) {
            const std::wstring title = Loc(kColumnKeys[index]);
            LVCOLUMNW column = {};
            column.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM | LVCF_FMT;
            column.fmt = LVCFMT_LEFT;
            column.cx = S(kColumnWidths[index]);
            column.iSubItem = index;
            column.pszText = const_cast<LPWSTR>(title.c_str());
            ListView_InsertColumn(list_, index, &column);
        }
        ::SetWindowSubclass(list_, &ShortcutManagerWindow::ListSubclassProc, 1,
                            reinterpret_cast<DWORD_PTR>(this));
    }

    const int right = kWindowWidth - kListX;
    const int ok_x = right - kRowButtonWidth;
    const int down_x = ok_x - kRowButtonGap - kRowButtonWidth;
    const int up_x = down_x - kRowButtonGap - kRowButtonWidth;
    const int delete_x = up_x - kRowButtonGap - kRowButtonWidth;
    const int edit_x = delete_x - kRowButtonGap - kRowButtonWidth;
    const int add_x = edit_x - kRowButtonGap - kRowButtonWidth;

    add_button_ = MakeControl(WC_BUTTONW, Loc("Common.BtnAdd"),
                              WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                              add_x, kRowButtonY, kRowButtonWidth,
                              kRowButtonHeight, kIdAdd);
    edit_button_ = MakeControl(WC_BUTTONW, Loc("Common.BtnEdit"),
                               WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                               edit_x, kRowButtonY, kRowButtonWidth,
                               kRowButtonHeight, kIdEdit);
    delete_button_ = MakeControl(
        WC_BUTTONW, Loc("Common.BtnDelete"),
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, delete_x,
        kRowButtonY, kRowButtonWidth, kRowButtonHeight, kIdDelete);
    up_button_ = MakeControl(WC_BUTTONW, Loc("resMoveUp"),
                             WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                             up_x, kRowButtonY, kRowButtonWidth, kRowButtonHeight,
                             kIdUp);
    down_button_ = MakeControl(
        WC_BUTTONW, Loc("resMoveDown"),
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, down_x, kRowButtonY,
        kRowButtonWidth, kRowButtonHeight, kIdDown);
    ok_button_ = MakeControl(
        WC_BUTTONW, Loc("Common.BtnOK"),
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
        ok_x, kRowButtonY, kRowButtonWidth, kRowButtonHeight, kIdOk);

    // 导入/导出 live in the bottom-right corner; the item actions share the row
    // above them.
    const int small_row_right = right - kSmallButtonWidth;
    const int import_x = small_row_right;
    const int export_x = import_x - kRowButtonGap - kSmallButtonWidth;
    export_button_ = MakeControl(
        WC_BUTTONW, Loc("resExport"),
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, export_x,
        kSmallButtonY, kSmallButtonWidth, kSmallButtonHeight, kIdExport);
    import_button_ = MakeControl(
        WC_BUTTONW, Loc("resImport"), WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                                          BS_PUSHBUTTON,
        import_x, kSmallButtonY, kSmallButtonWidth, kSmallButtonHeight, kIdImport);

    hint_label_ = MakeControl(
        WC_STATICW, Loc("resManagerHint"),
        WS_CHILD | WS_VISIBLE | SS_LEFT | SS_CENTERIMAGE, kListX, kSmallButtonY,
        560, kSmallButtonHeight, 0);

    HWND buttons[] = {add_button_,    edit_button_,   delete_button_,
                      up_button_,     down_button_,   ok_button_,
                      export_button_, import_button_};
    for (HWND button : buttons) {
        if (button != nullptr) {
            ::SetWindowSubclass(button, &ShortcutManagerWindow::ButtonSubclassProc,
                                1, reinterpret_cast<DWORD_PTR>(this));
        }
    }
}

void ShortcutManagerWindow::OnCreate() {
    dpi_ = static_cast<int>(::GetDpiForWindow(hwnd_));
    if (dpi_ <= 0) {
        dpi_ = USER_DEFAULT_SCREEN_DPI;
    }
    CreateFonts();
    background_brush_ = ::CreateSolidBrush(::GetSysColor(COLOR_BTNFACE));
    CreateControls();
    ReloadList();
}

std::wstring ShortcutManagerWindow::ParamTypeLabel(ParamType type) {
    switch (type) {
        case ParamType::None:
            return Loc("ShortCutForm.ParamType_0");
        case ParamType::NoEncoding:
            return Loc("ShortCutForm.ParamType_1");
        case ParamType::UrlQuery:
            return Loc("ShortCutForm.ParamType_2");
        case ParamType::Utf8Query:
            return Loc("ShortCutForm.ParamType_3");
    }
    return {};
}

void ShortcutManagerWindow::ReloadList(int select_row) {
    if (list_ == nullptr) {
        return;
    }
    const int top_row = ListView_GetTopIndex(list_);
    ListView_DeleteAllItems(list_);

    const std::vector<ShortcutEntry>& entries = store_.entries();
    for (size_t index = 0; index < entries.size(); ++index) {
        const ShortcutEntry& entry = entries[index];
        const std::wstring keyword =
            entry.is_blank ? std::wstring(L"—") : ToWide(entry.item.keyword);
        const std::wstring name =
            entry.is_blank ? Loc("resBlankRow") : ToWide(entry.item.name);
        const std::wstring param_type =
            entry.is_blank ? std::wstring() : ParamTypeLabel(entry.item.param_type);
        const std::wstring command_line =
            entry.is_blank ? std::wstring() : ToWide(entry.item.command_line);

        LVITEMW row = {};
        row.mask = LVIF_TEXT;
        row.iItem = static_cast<int>(index);
        row.iSubItem = 0;
        row.pszText = const_cast<LPWSTR>(keyword.c_str());
        const int inserted = ListView_InsertItem(list_, &row);
        if (inserted < 0) {
            continue;
        }
        ListView_SetItemText(list_, inserted, 1, const_cast<LPWSTR>(name.c_str()));
        ListView_SetItemText(list_, inserted, 2,
                             const_cast<LPWSTR>(param_type.c_str()));
        ListView_SetItemText(list_, inserted, 3,
                             const_cast<LPWSTR>(command_line.c_str()));
    }

    if (select_row >= 0) {
        SelectRow(select_row);
    } else if (top_row > 0) {
        ListView_EnsureVisible(list_, top_row, FALSE);
    }
    UpdateButtonStates();
}

int ShortcutManagerWindow::SelectedRow() const {
    if (list_ == nullptr) {
        return -1;
    }
    return ListView_GetNextItem(list_, -1, LVNI_SELECTED);
}

void ShortcutManagerWindow::SelectRow(int row) {
    if (list_ == nullptr) {
        return;
    }
    ListView_SetItemState(list_, -1, 0, LVIS_SELECTED | LVIS_FOCUSED);
    const int count = ListView_GetItemCount(list_);
    if (row >= 0 && row < count) {
        ListView_SetItemState(list_, row, LVIS_SELECTED | LVIS_FOCUSED,
                              LVIS_SELECTED | LVIS_FOCUSED);
        ListView_EnsureVisible(list_, row, FALSE);
    }
    UpdateButtonStates();
}

void ShortcutManagerWindow::UpdateButtonStates() {
    const int count = list_ != nullptr ? ListView_GetItemCount(list_) : 0;
    const int selection = SelectedRow();
    if (edit_button_ != nullptr) {
        ::EnableWindow(edit_button_, selection >= 0 ? TRUE : FALSE);
    }
    if (delete_button_ != nullptr) {
        ::EnableWindow(delete_button_, selection >= 0 ? TRUE : FALSE);
    }
    if (up_button_ != nullptr) {
        ::EnableWindow(up_button_, selection > 0 ? TRUE : FALSE);
    }
    if (down_button_ != nullptr) {
        ::EnableWindow(down_button_,
                       (selection >= 0 && selection + 1 < count) ? TRUE : FALSE);
    }
}

bool ShortcutManagerWindow::SameItem(const Shortcut& left, const Shortcut& right) {
    return left.param_type == right.param_type && left.keyword == right.keyword &&
           left.name == right.name && left.command_line == right.command_line;
}

bool ShortcutManagerWindow::IsDuplicate(const Shortcut& item, int ignore_index) const {
    const std::vector<ShortcutEntry>& entries = store_.entries();
    for (size_t index = 0; index < entries.size(); ++index) {
        if (static_cast<int>(index) == ignore_index) {
            continue;
        }
        if (entries[index].is_blank) {
            continue;
        }
        if (SameItem(entries[index].item, item)) {
            return true;
        }
    }
    return false;
}

void ShortcutManagerWindow::SaveAndNotify() {
    if (!store_.Save()) {
        Logger::Instance().Error("shortcut manager: saving shortcut list failed");
        MessageBoxW(hwnd_, Loc("resSaveListFailed").c_str(),
                    Loc("ShortCutManForm.ShortCutManFormCaption").c_str(),
                    MB_OK | MB_ICONERROR);
    }
    if (changed_handler_) {
        changed_handler_();
    }
}

void ShortcutManagerWindow::AddItem() {
    if (edit_window_ == nullptr) {
        return;
    }
    edit_window_->Open(nullptr, [this](const Shortcut& item, bool is_blank) {
        if (!is_blank && IsDuplicate(item, -1)) {
            MessageBoxW(hwnd_, Loc("resShortcutExists").c_str(),
                        Loc("ShortCutManForm.ShortCutManFormCaption").c_str(),
                        MB_OK | MB_ICONWARNING);
            return;
        }
        ShortcutEntry entry;
        entry.is_blank = is_blank;
        if (!is_blank) {
            entry.item = item;
        }
        store_.mutable_entries().push_back(std::move(entry));
        SaveAndNotify();
        ReloadList(static_cast<int>(store_.entries().size()) - 1);
    });
}

void ShortcutManagerWindow::EditSelected() {
    const int index = SelectedRow();
    if (index < 0 || edit_window_ == nullptr) {
        return;
    }
    const std::vector<ShortcutEntry>& entries = store_.entries();
    if (static_cast<size_t>(index) >= entries.size()) {
        return;
    }
    const bool was_blank = entries[static_cast<size_t>(index)].is_blank;
    Shortcut current;
    if (!was_blank) {
        current = entries[static_cast<size_t>(index)].item;
    }
    edit_window_->Open(was_blank ? nullptr : &current,
                       [this, index](const Shortcut& item, bool is_blank) {
                           std::vector<ShortcutEntry>& target =
                               store_.mutable_entries();
                           if (index < 0 ||
                               static_cast<size_t>(index) >= target.size()) {
                               return;
                           }
                           if (!is_blank && IsDuplicate(item, index)) {
                               MessageBoxW(hwnd_, Loc("resShortcutExists").c_str(),
                                           Loc("ShortCutManForm.ShortCutManFormCaption").c_str(),
                                           MB_OK | MB_ICONWARNING);
                               return;
                           }
                           ShortcutEntry updated;
                           updated.is_blank = is_blank;
                           if (!is_blank) {
                               updated.item = item;
                           }
                           target[static_cast<size_t>(index)] = std::move(updated);
                           SaveAndNotify();
                           ReloadList(index);
                       });
}

void ShortcutManagerWindow::DeleteSelected() {
    const int index = SelectedRow();
    if (index < 0) {
        return;
    }
    const std::vector<ShortcutEntry>& entries = store_.entries();
    if (static_cast<size_t>(index) >= entries.size()) {
        return;
    }
    const ShortcutEntry& entry = entries[static_cast<size_t>(index)];
    std::wstring question;
    if (entry.is_blank) {
        question = Loc("ShortCutManForm.DeleteBlankLine");
    } else {
        question = LocArg(Loc("resDeleteConfirm"),
                          ToWide(entry.item.DisplayName()));
    }
    if (MessageBoxW(hwnd_, question.c_str(), Loc("resTitleDelete").c_str(),
                    MB_YESNO | MB_ICONQUESTION) != IDYES) {
        return;
    }
    store_.RemoveEntry(static_cast<size_t>(index));
    SaveAndNotify();
    const int count = static_cast<int>(store_.entries().size());
    ReloadList(std::min(index, count - 1));
}

void ShortcutManagerWindow::MoveSelected(int delta) {
    const int index = SelectedRow();
    if (index < 0 || delta == 0) {
        return;
    }
    const int count = static_cast<int>(store_.entries().size());
    const int target = index + delta;
    if (target < 0 || target >= count) {
        return;
    }
    std::vector<ShortcutEntry>& entries = store_.mutable_entries();
    std::swap(entries[static_cast<size_t>(index)],
              entries[static_cast<size_t>(target)]);
    SaveAndNotify();
    ReloadList(target);
}

bool ShortcutManagerWindow::MoveRow(int from, int before) {
    std::vector<ShortcutEntry>& entries = store_.mutable_entries();
    const int count = static_cast<int>(entries.size());
    if (from < 0 || from >= count || before < 0 || before > count) {
        return false;
    }
    if (before == from || before == from + 1) {
        return false;  // dropped onto itself: nothing to do
    }
    ShortcutEntry moved = std::move(entries[static_cast<size_t>(from)]);
    entries.erase(entries.begin() + static_cast<std::ptrdiff_t>(from));
    if (before > from) {
        --before;
    }
    before = std::clamp(before, 0, static_cast<int>(entries.size()));
    entries.insert(entries.begin() + static_cast<std::ptrdiff_t>(before),
                   std::move(moved));
    SaveAndNotify();
    ReloadList(before);
    return true;
}

void ShortcutManagerWindow::BeginDrag(int row) {
    if (row < 0 || list_ == nullptr) {
        return;
    }
    dragging_ = true;
    drag_from_ = row;
    drag_target_ = row;
    ::SetCapture(hwnd_);
}

void ShortcutManagerWindow::UpdateDragTarget(LPARAM lparam) {
    if (!dragging_ || list_ == nullptr) {
        return;
    }
    POINT point = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
    if (::ClientToScreen(hwnd_, &point) == FALSE) {
        return;
    }
    if (::ScreenToClient(list_, &point) == FALSE) {
        return;
    }
    LVHITTESTINFO info = {};
    info.pt = point;
    const int row = ListView_HitTest(list_, &info);
    // Rows are inserted *before* the hit row; dropping on empty space is
    // ignored, mirroring the original manager (FSCM:634).
    drag_target_ = row >= 0 ? row : -1;
}

void ShortcutManagerWindow::EndDrag() {
    if (!dragging_) {
        return;
    }
    const int from = drag_from_;
    const int target = drag_target_;
    dragging_ = false;
    drag_from_ = -1;
    drag_target_ = -1;
    if (::GetCapture() == hwnd_) {
        ::ReleaseCapture();
    }
    if (from < 0 || target < 0) {
        return;
    }
    MoveRow(from, target);
}

void ShortcutManagerWindow::ShowContextMenu(POINT screen_position) {
    HMENU menu = ::CreatePopupMenu();
    if (menu == nullptr) {
        return;
    }
    ::AppendMenuW(menu, MF_STRING, kIdAdd, Loc("resMenuAdd").c_str());
    ::AppendMenuW(menu, MF_STRING, kIdEdit, Loc("resMenuEdit").c_str());
    ::AppendMenuW(menu, MF_STRING, kIdDelete, Loc("resMenuDelete").c_str());
    if (SelectedRow() < 0) {
        ::EnableMenuItem(menu, kIdEdit, MF_BYCOMMAND | MF_GRAYED);
        ::EnableMenuItem(menu, kIdDelete, MF_BYCOMMAND | MF_GRAYED);
    }
    ::SetForegroundWindow(hwnd_);
    const int command = ::TrackPopupMenu(
        menu,
        TPM_LEFTBUTTON | TPM_RIGHTBUTTON | TPM_RETURNCMD | TPM_NONOTIFY,
        screen_position.x, screen_position.y, 0, hwnd_, nullptr);
    ::DestroyMenu(menu);
    ::PostMessageW(hwnd_, WM_NULL, 0, 0);
    switch (command) {
        case kIdAdd:
            AddItem();
            break;
        case kIdEdit:
            EditSelected();
            break;
        case kIdDelete:
            DeleteSelected();
            break;
        default:
            break;
    }
}

bool ShortcutManagerWindow::ReadFileBytes(const std::wstring& path,
                                          std::string* bytes) {
    if (bytes == nullptr) {
        return false;
    }
    bytes->clear();
    HANDLE file = ::CreateFileW(path.c_str(), GENERIC_READ,
                                FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                                OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }
    LARGE_INTEGER size = {};
    if (::GetFileSizeEx(file, &size) == FALSE ||
        size.QuadPart < 0 ||
        static_cast<unsigned long long>(size.QuadPart) >= kMaxImportFileSize) {
        ::CloseHandle(file);
        return false;
    }
    if (size.QuadPart > 0) {
        bytes->resize(static_cast<size_t>(size.QuadPart));
        DWORD read = 0;
        if (::ReadFile(file, bytes->data(), static_cast<DWORD>(bytes->size()), &read,
                       nullptr) == FALSE) {
            bytes->clear();
            ::CloseHandle(file);
            return false;
        }
        bytes->resize(read);
    }
    ::CloseHandle(file);
    return true;
}

void ShortcutManagerWindow::ExportToFile() {
    const std::wstring target = PickSaveFileNative(
        hwnd_, Loc("resExportPickTitle").c_str(), L"ShortCutList.txt");
    if (target.empty()) {
        return;
    }
    const std::wstring source = store_.list_path();
    if (::CompareStringOrdinal(source.c_str(), -1, target.c_str(), -1, TRUE) ==
        CSTR_EQUAL) {
        MessageBoxW(hwnd_, Loc("resExportSameFile").c_str(),
                    Loc("resTitleExport").c_str(), MB_OK | MB_ICONINFORMATION);
        return;
    }
    // Make sure the on-disk file matches the in-memory list before copying it.
    if (!store_.Save()) {
        MessageBoxW(hwnd_, Loc("resExportSaveFailed").c_str(),
                    Loc("resTitleExport").c_str(), MB_OK | MB_ICONERROR);
        return;
    }
    if (::CopyFileW(source.c_str(), target.c_str(), FALSE) == FALSE) {
        const std::wstring text =
            Loc("resExportFailedCode") + std::to_wstring(::GetLastError());
        MessageBoxW(hwnd_, text.c_str(), Loc("resTitleExport").c_str(),
                    MB_OK | MB_ICONERROR);
        return;
    }
    const std::wstring text = Loc("resExportDone") + L"\r\n" + target;
    MessageBoxW(hwnd_, text.c_str(), Loc("resTitleExport").c_str(),
                MB_OK | MB_ICONINFORMATION);
}

void ShortcutManagerWindow::ImportFromFile() {
    const std::wstring path =
        PickOpenFileNative(hwnd_, Loc("resImportPickTitle").c_str());
    if (path.empty()) {
        return;
    }
    std::string bytes;
    if (!ReadFileBytes(path, &bytes)) {
        MessageBoxW(hwnd_, Loc("resImportReadFailed").c_str(),
                    Loc("resTitleImport").c_str(), MB_OK | MB_ICONERROR);
        return;
    }

    // Encoding detection matches ShortcutStore::Load(): UTF-8/UTF-16 BOM first,
    // then strict UTF-8 validation, otherwise the legacy ANSI/GBK code page.
    std::string utf8;
    if (bytes.size() >= 3 && static_cast<unsigned char>(bytes[0]) == 0xEF &&
        static_cast<unsigned char>(bytes[1]) == 0xBB &&
        static_cast<unsigned char>(bytes[2]) == 0xBF) {
        utf8 = bytes.substr(3);
    } else if (bytes.size() >= 2 && static_cast<unsigned char>(bytes[0]) == 0xFF &&
               static_cast<unsigned char>(bytes[1]) == 0xFE) {
        const std::wstring wide(
            reinterpret_cast<const wchar_t*>(bytes.data() + 2),
            (bytes.size() - 2) / sizeof(wchar_t));
        utf8 = ToUtf8(wide);
    } else if (IsValidUtf8(bytes)) {
        utf8 = bytes;
    } else {
        utf8 = AnsiToUtf8(bytes);
    }
    ReplaceFromImportedText(utf8);
}

void ShortcutManagerWindow::ReplaceFromImportedText(const std::string& text) {
    std::vector<ShortcutEntry> parsed;
    size_t start = 0;
    while (start <= text.size()) {
        size_t next = text.find('\n', start);
        const bool last_line = next == std::string::npos;
        if (last_line) {
            next = text.size();
        }
        std::string line = text.substr(start, next - start);
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        // A final empty segment is just the trailing CRLF of the file; adding
        // it would add a phantom blank row on every import.
        if (!(last_line && line.empty())) {
            ShortcutEntry entry;
            if (ShortcutStore::ParseLine(line, &entry)) {
                parsed.push_back(std::move(entry));
            }
        }
        if (last_line) {
            break;
        }
        start = next + 1;
    }

    if (parsed.empty()) {
        MessageBoxW(hwnd_, Loc("resImportNothing").c_str(),
                    Loc("resTitleImport").c_str(), MB_OK | MB_ICONWARNING);
        return;
    }

    const std::wstring question =
        Loc("resImportConfirm") + L"\r\n\r\n" + Loc("resImportCurrent") +
        std::to_wstring(store_.entries().size()) + Loc("resImportRows") +
        std::to_wstring(CountItems(store_.entries())) + Loc("resImportItems") +
        L"\r\n" + Loc("resImportIncoming") + std::to_wstring(parsed.size()) +
        Loc("resImportRows") + std::to_wstring(CountItems(parsed)) +
        Loc("resImportItems") + L"\r\n\r\n" + Loc("resImportContinue");
    if (MessageBoxW(hwnd_, question.c_str(), Loc("resTitleImport").c_str(),
                    MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) != IDYES) {
        return;
    }

    store_.mutable_entries() = std::move(parsed);
    SaveAndNotify();
    ReloadList(0);
    Logger::Instance().Info("shortcut manager: imported shortcut list");
}

void ShortcutManagerWindow::Open() {
    if (hwnd_ == nullptr) {
        return;
    }
    ReloadList();
    PlaceOnCursorMonitor();
    ::ShowWindow(hwnd_, SW_SHOWNORMAL);
    ::SetForegroundWindow(hwnd_);
    if (list_ != nullptr) {
        ::SetFocus(list_);
    }
    ::InvalidateRect(hwnd_, nullptr, TRUE);
    Logger::Instance().Trace("shortcut manager opened");
}

void ShortcutManagerWindow::PlaceOnCursorMonitor() {
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
    int x = info.rcWork.left + (work_width - width) / 2;
    int y = info.rcWork.top + (work_height - height) / 2;
    x = std::max(x, static_cast<int>(info.rcWork.left));
    y = std::max(y, static_cast<int>(info.rcWork.top));
    ::SetWindowPos(hwnd_, HWND_TOPMOST, x, y, width, height, SWP_NOACTIVATE);
}

void ShortcutManagerWindow::MoveFocus(bool backwards) {
    if (hwnd_ == nullptr) {
        return;
    }
    const HWND order[] = {list_,
                          add_button_,
                          edit_button_,
                          delete_button_,
                          up_button_,
                          down_button_,
                          ok_button_,
                          export_button_,
                          import_button_};
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

    const HWND focused = ::GetFocus();
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

void ShortcutManagerWindow::DrawHeader(HDC dc) const {
    if (font_ == nullptr || title_font_ == nullptr) {
        return;
    }
    RECT client = {};
    ::GetClientRect(hwnd_, &client);

    ::SetBkMode(dc, TRANSPARENT);
    ::SetTextColor(dc, ::GetSysColor(COLOR_BTNTEXT));
    HGDIOBJ previous = ::SelectObject(dc, title_font_);
    RECT title_rect = {S(20), S(4), client.right - S(20), S(kHeaderHeight - 6)};
    const std::wstring header = Loc("ShortCutManForm.ShortCutManFormCaption");
    ::DrawTextW(dc, header.c_str(), -1, &title_rect,
                DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
    ::SelectObject(dc, previous);

    const RECT separator = {S(16), S(kHeaderHeight), client.right - S(16),
                            S(kHeaderHeight) + 1};
    ::FillRect(dc, &separator, ::GetSysColorBrush(COLOR_3DSHADOW));
}

void ShortcutManagerWindow::OnPaint() {
    PAINTSTRUCT paint = {};
    HDC dc = ::BeginPaint(hwnd_, &paint);
    DrawHeader(dc);
    ::EndPaint(hwnd_, &paint);
}

LRESULT ShortcutManagerWindow::HandleMessage(UINT message, WPARAM wparam,
                                             LPARAM lparam) {
    switch (message) {
        case WM_COMMAND: {
            if (HIWORD(wparam) != BN_CLICKED) {
                return 0;
            }
            switch (LOWORD(wparam)) {
                case kIdAdd:
                    AddItem();
                    return 0;
                case kIdEdit:
                    EditSelected();
                    return 0;
                case kIdDelete:
                    DeleteSelected();
                    return 0;
                case kIdUp:
                    MoveSelected(-1);
                    return 0;
                case kIdDown:
                    MoveSelected(1);
                    return 0;
                case kIdOk:
                    Hide();
                    return 0;
                case kIdExport:
                    ExportToFile();
                    return 0;
                case kIdImport:
                    ImportFromFile();
                    return 0;
                default:
                    return 0;
            }
        }
        case WM_NOTIFY: {
            const NMHDR* header = reinterpret_cast<const NMHDR*>(lparam);
            if (header == nullptr || header->hwndFrom != list_) {
                break;
            }
            switch (header->code) {
                case LVN_BEGINDRAG: {
                    const NMLISTVIEW* info =
                        reinterpret_cast<const NMLISTVIEW*>(lparam);
                    BeginDrag(info->iItem);
                    return 0;
                }
                case NM_DBLCLK: {
                    const NMITEMACTIVATE* info =
                        reinterpret_cast<const NMITEMACTIVATE*>(lparam);
                    if (info->iItem >= 0) {
                        EditSelected();
                    }
                    return 0;
                }
                case LVN_ITEMCHANGED:
                    UpdateButtonStates();
                    return 0;
                default:
                    break;
            }
            break;
        }
        case WM_MOUSEMOVE:
            if (dragging_) {
                UpdateDragTarget(lparam);
                return 0;
            }
            break;
        case WM_LBUTTONUP:
            if (dragging_) {
                EndDrag();
                return 0;
            }
            break;
        case WM_CAPTURECHANGED:
            // Anybody else stealing the capture (e.g. a message box) cancels
            // the drag without touching the list order.
            dragging_ = false;
            drag_from_ = -1;
            drag_target_ = -1;
            break;
        case WM_CONTEXTMENU: {
            if (reinterpret_cast<HWND>(wparam) != list_ || list_ == nullptr) {
                break;
            }
            POINT position = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
            if (position.x == -1 && position.y == -1) {
                // Keyboard menu key: show it at the list's top-left corner.
                position = {0, 0};
                ::ClientToScreen(list_, &position);
            } else {
                POINT client = position;
                ::ScreenToClient(list_, &client);
                LVHITTESTINFO info = {};
                info.pt = client;
                const int row = ListView_HitTest(list_, &info);
                if (row >= 0) {
                    SelectRow(row);
                }
            }
            ShowContextMenu(position);
            return 0;
        }
        case WM_CLOSE:
            EndDrag();
            Hide();
            return 0;
        case WM_KEYDOWN:
            if (wparam == VK_ESCAPE) {
                Hide();
                return 0;
            }
            if (wparam == VK_TAB) {
                MoveFocus((::GetKeyState(VK_SHIFT) & 0x8000) != 0);
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
        case WM_CTLCOLORSTATIC: {
            HDC dc = reinterpret_cast<HDC>(wparam);
            ::SetBkMode(dc, TRANSPARENT);
            return reinterpret_cast<LRESULT>(background_brush_);
        }
        case WM_DESTROY:
            // Also covered by the destructor; kept for an explicit Destroy().
            if (edit_window_ != nullptr) {
                edit_window_->Hide();
            }
            DestroyResources();
            return 0;
        default:
            break;
    }
    return Window::HandleMessage(message, wparam, lparam);
}

LRESULT CALLBACK ShortcutManagerWindow::ListSubclassProc(HWND hwnd, UINT message,
                                                         WPARAM wparam, LPARAM lparam,
                                                         UINT_PTR,
                                                         DWORD_PTR ref_data) {
    auto* self = reinterpret_cast<ShortcutManagerWindow*>(ref_data);
    if (self == nullptr) {
        return ::DefSubclassProc(hwnd, message, wparam, lparam);
    }
    switch (message) {
        case WM_KEYDOWN: {
            const UINT key = static_cast<UINT>(wparam);
            if (key == VK_DELETE) {
                ::PostMessageW(self->hwnd_, WM_COMMAND,
                               MAKEWPARAM(kIdDelete, BN_CLICKED), 0);
                return 0;
            }
            if (key == VK_INSERT) {
                ::PostMessageW(self->hwnd_, WM_COMMAND,
                               MAKEWPARAM(kIdAdd, BN_CLICKED), 0);
                return 0;
            }
            if (key == VK_F2 || key == VK_RETURN) {
                ::PostMessageW(self->hwnd_, WM_COMMAND,
                               MAKEWPARAM(kIdEdit, BN_CLICKED), 0);
                return 0;
            }
            if (key == VK_ESCAPE) {
                ::PostMessageW(self->hwnd_, WM_CLOSE, 0, 0);
                return 0;
            }
            if (key == VK_TAB) {
                self->MoveFocus((::GetKeyState(VK_SHIFT) & 0x8000) != 0);
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

LRESULT CALLBACK ShortcutManagerWindow::ButtonSubclassProc(
    HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam, UINT_PTR,
    DWORD_PTR ref_data) {
    auto* self = reinterpret_cast<ShortcutManagerWindow*>(ref_data);
    if (self == nullptr) {
        return ::DefSubclassProc(hwnd, message, wparam, lparam);
    }
    switch (message) {
        case WM_KEYDOWN: {
            const UINT key = static_cast<UINT>(wparam);
            if (key == VK_TAB) {
                self->MoveFocus((::GetKeyState(VK_SHIFT) & 0x8000) != 0);
                return 0;
            }
            if (key == VK_ESCAPE) {
                ::PostMessageW(self->hwnd_, WM_CLOSE, 0, 0);
                return 0;
            }
            if (key == VK_RETURN) {
                ::SendMessageW(hwnd, BM_CLICK, 0, 0);
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
