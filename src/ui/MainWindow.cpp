#include "ui/MainWindow.h"

#include <imm.h>
#include <dwmapi.h>
#include <windowsx.h>

#include <algorithm>
#include <cctype>

#include "core/Formatter.h"
#include "core/Launcher.h"
#include "core/Logger.h"
#include "core/Text.h"
#include "ui/ParamWindow.h"

namespace alt {
namespace {

constexpr float kPad = 12.0f;
constexpr float kHeaderHeight = 26.0f;
constexpr float kInputHeight = 40.0f;
constexpr float kRowHeight = 30.0f;
constexpr float kDetailHeight = 22.0f;
constexpr float kFooterHeight = 20.0f;
constexpr size_t kTopTenRows = 10;
constexpr size_t kMaxVisibleRows = 16;

// The original stored its fonts as INI strings ("字体名|[]|12|65535|0"); the
// parser arrives with the config milestone, these are the effective defaults.
constexpr float kQueryFontSize = 18.0f;
constexpr float kListFontSize = 15.0f;

constexpr UINT kTimerCaret = 1;
constexpr UINT kTimerHide = 2;
constexpr UINT kCaretBlinkMs = 530;

// Posted to the window when ExecuteIfOnlyOne should run the single match;
// posting avoids re-entrancy while the result list is being rebuilt.
constexpr UINT kMessageAutoExecute = WM_APP + 20;

const D2D1_COLOR_F kBackground = D2D1::ColorF(0.102f, 0.110f, 0.125f, 1.0f);
const D2D1_COLOR_F kBorder = D2D1::ColorF(0.239f, 0.255f, 0.290f, 1.0f);
const D2D1_COLOR_F kSelected = D2D1::ColorF(0.161f, 0.290f, 0.435f, 1.0f);
const D2D1_COLOR_F kHover = D2D1::ColorF(0.145f, 0.157f, 0.176f, 1.0f);
const D2D1_COLOR_F kTextBright = D2D1::ColorF(0.925f, 0.937f, 0.957f, 1.0f);
const D2D1_COLOR_F kTextDim = D2D1::ColorF(0.541f, 0.569f, 0.612f, 1.0f);
const D2D1_COLOR_F kAccent = D2D1::ColorF(0.361f, 0.659f, 0.988f, 1.0f);
const D2D1_COLOR_F kCaret = D2D1::ColorF(0.361f, 0.659f, 0.988f, 1.0f);

}  // namespace

MainWindow::MainWindow(Settings& settings, ShortcutStore& store,
                       ParamHistory& param_history)
    : settings_(settings), store_(store), param_history_(param_history) {}

MainWindow::~MainWindow() = default;

bool MainWindow::CreateMain(HINSTANCE instance) {
    if (!Create(instance, L"ALTRun.MainWindow.v2", L"ALTRun", WS_POPUP,
                WS_EX_TOOLWINDOW | WS_EX_TOPMOST)) {
        return false;
    }
    // The parameter prompt is created lazily on first use but its class is
    // registered here so the first Open() has no extra latency.
    param_window_ = std::make_unique<ParamWindow>(settings_, param_history_);
    if (!param_window_->CreateParamWindow(instance)) {
        Logger::Instance().Warn("cannot create parameter window");
        param_window_.reset();
    }
    return true;
}

void MainWindow::OnCreate() {
    title_format_.Attach(d2d_.CreateFormat(11.0f, DWRITE_FONT_WEIGHT_SEMI_BOLD));
    query_format_.Attach(
        d2d_.CreateFormat(kQueryFontSize, DWRITE_FONT_WEIGHT_NORMAL));
    row_format_.Attach(
        d2d_.CreateFormat(kListFontSize, DWRITE_FONT_WEIGHT_NORMAL));
    row_bold_format_.Attach(
        d2d_.CreateFormat(kListFontSize, DWRITE_FONT_WEIGHT_SEMI_BOLD));
    hint_format_.Attach(d2d_.CreateFormat(11.0f, DWRITE_FONT_WEIGHT_NORMAL));
    mono_format_.Attach(d2d_.CreateFormat(kListFontSize,
                                          DWRITE_FONT_WEIGHT_NORMAL));

    ::SetTimer(hwnd_, kTimerCaret, kCaretBlinkMs, nullptr);
    d2d_.Initialize(hwnd_);
    UpdateWindowAlpha();
    Relayout();

    const DWORD corner_pref = 2;  // DWMWCP_ROUND (Windows 11)
    ::DwmSetWindowAttribute(hwnd_, 33, &corner_pref, sizeof(corner_pref));
    const BOOL dark = TRUE;
    ::DwmSetWindowAttribute(hwnd_, 20, &dark, sizeof(dark));
}

void MainWindow::UpdateWindowAlpha() {
    const int alpha = std::clamp(settings_.alpha, 0, 255);
    LONG_PTR ex = ::GetWindowLongPtrW(hwnd_, GWL_EXSTYLE);
    if (alpha < 255) {
        ::SetWindowLongPtrW(hwnd_, GWL_EXSTYLE, ex | WS_EX_LAYERED);
        ::SetLayeredWindowAttributes(hwnd_, 0, static_cast<BYTE>(alpha), LWA_ALPHA);
    } else if ((ex & WS_EX_LAYERED) != 0) {
        ::SetWindowLongPtrW(hwnd_, GWL_EXSTYLE, ex & ~WS_EX_LAYERED);
    }
}

size_t MainWindow::VisibleRows() const {
    const size_t limit = settings_.show_top_ten ? kTopTenRows : kMaxVisibleRows;
    return std::min(matches_.size(), limit);
}

float MainWindow::ContentHeight() const {
    return kPad + kHeaderHeight + kInputHeight + VisibleRows() * kRowHeight +
           kDetailHeight + kFooterHeight + kPad;
}

void MainWindow::Relayout() {
    const int width = std::clamp(settings_.form_width, 280, 1400);
    const int height = static_cast<int>(ContentHeight());
    RECT current = {};
    ::GetWindowRect(hwnd_, &current);
    const bool size_changed = (current.right - current.left) != width ||
                              (current.bottom - current.top) != height;
    if (size_changed || !visible()) {
        PlaceOnCursorMonitor();
    }
    ::SetWindowPos(hwnd_, HWND_TOPMOST, 0, 0, width, height,
                   SWP_NOMOVE | SWP_NOACTIVATE);
    d2d_.Resize(static_cast<UINT>(width), static_cast<UINT>(height));
}

void MainWindow::PlaceOnCursorMonitor() {
    POINT cursor = {};
    ::GetCursorPos(&cursor);
    HMONITOR monitor = ::MonitorFromPoint(cursor, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = {};
    mi.cbSize = sizeof(mi);
    ::GetMonitorInfoW(monitor, &mi);

    RECT current = {};
    ::GetWindowRect(hwnd_, &current);
    const int width = current.right - current.left;
    const int height = current.bottom - current.top;

    const int work_width = mi.rcWork.right - mi.rcWork.left;
    const int work_height = mi.rcWork.bottom - mi.rcWork.top;
    const int x = mi.rcWork.left + (work_width - width) / 2;
    // The original launcher sits slightly above centre.
    const int y = mi.rcWork.top + (work_height - height) / 2 - work_height / 10;

    ::SetWindowPos(hwnd_, HWND_TOPMOST, x, y, width, height,
                   SWP_NOACTIVATE | SWP_NOSIZE);
}

void MainWindow::ToggleLauncher() {
    if (visible()) {
        HideLauncher();
    } else {
        ShowLauncher();
    }
}

void MainWindow::ShowLauncher() {
    if (hwnd_ == nullptr) {
        return;
    }
    ClearQuery();
    RefreshMatches();
    PlaceOnCursorMonitor();
    Relayout();
    ::ShowWindow(hwnd_, SW_SHOWNORMAL);
    ::SetForegroundWindow(hwnd_);
    ::SetFocus(hwnd_);
    caret_visible_ = true;
    ResetHideTimer();
    ::InvalidateRect(hwnd_, nullptr, FALSE);
}

void MainWindow::ResetHideTimer() {
    ::KillTimer(hwnd_, kTimerHide);
    const int delay = settings_.hide_delay;
    if (delay > 0) {
        ::SetTimer(hwnd_, kTimerHide, static_cast<UINT>(delay) * 1000, nullptr);
    }
}

void MainWindow::HideLauncher() {
    if (hwnd_ == nullptr) {
        return;
    }
    if (::GetFocus() == hwnd_) {
        ::SetFocus(nullptr);
    }
    ::ShowWindow(hwnd_, SW_HIDE);
}

void MainWindow::ClearQuery() {
    query_.clear();
    ime_composition_.clear();
    selected_ = 0;
    hover_ = -1;
}

void MainWindow::RefreshMatches() {
    // While the recent list is displayed (Ctrl+L) an empty query keeps it
    // visible; typing re-enters normal filtering.
    if (showing_recent_ && query_.empty()) {
        return;
    }
    showing_recent_ = false;
    matches_.clear();

    if (query_.empty()) {
        // Empty query: default list ordered by the original Sort() rank.
        std::vector<MatchResult> all;
        const std::vector<ShortcutEntry>& entries = store_.entries();
        for (size_t i = 0; i < entries.size(); ++i) {
            if (entries[i].is_blank || entries[i].item.keyword.empty()) {
                continue;
            }
            MatchResult result;
            result.entry_index = i;
            result.score = Matcher::SortRank(entries[i].item);
            all.push_back(result);
        }
        std::stable_sort(all.begin(), all.end(),
                         [&entries](const MatchResult& a, const MatchResult& b) {
                             if (a.score != b.score) return a.score > b.score;
                             return entries[a.entry_index].item.keyword <
                                    entries[b.entry_index].item.keyword;
                         });
        matches_ = std::move(all);
    } else {
        MatchQuery request;
        request.text = query_;
        request.regex_mode = settings_.regex_mode;
        request.match_anywhere = settings_.match_anywhere;
        matches_ = Matcher().Match(store_.entries(), request);

        // Favourite boost: if the same keyword previously launched a specific
        // item, put it first (RememberFavouratMatch).
        if (settings_.remember_favourite_match) {
            const std::string favourite = store_.FindFavourite(query_);
            if (!favourite.empty()) {
                for (size_t i = 0; i < matches_.size(); ++i) {
                    const Shortcut& item = store_.entries()[matches_[i].entry_index].item;
                    if (item.DisplayName() == favourite) {
                        if (i != 0) {
                            std::rotate(matches_.begin(),
                                        matches_.begin() + static_cast<std::ptrdiff_t>(i),
                                        matches_.begin() + static_cast<std::ptrdiff_t>(i) + 1);
                        }
                        break;
                    }
                }
            }
        }
    }

    if (selected_ >= static_cast<int>(matches_.size())) {
        selected_ = matches_.empty() ? 0 : static_cast<int>(matches_.size()) - 1;
    }
    Relayout();
    ::InvalidateRect(hwnd_, nullptr, FALSE);

    // ExecuteIfOnlyOne: a single match runs immediately, without waiting for
    // Enter (original behaviour, default off).
    if (settings_.execute_if_only_one && matches_.size() == 1 && !query_.empty()) {
        ::PostMessageW(hwnd_, kMessageAutoExecute, 0, 0);
    }
}

void MainWindow::ShowRecentList() {
    showing_recent_ = true;
    matches_.clear();
    const std::vector<ShortcutEntry>& entries = store_.entries();
    for (const size_t index : store_.latest()) {
        if (index >= entries.size() || entries[index].is_blank) {
            continue;
        }
        MatchResult result;
        result.entry_index = index;
        result.score = 0;
        matches_.push_back(result);
    }
    selected_ = 0;
    hover_ = -1;
    Relayout();
    ::InvalidateRect(hwnd_, nullptr, FALSE);
}

void MainWindow::ApplySettings() {
    UpdateWindowAlpha();
    Relayout();
    if (visible()) {
        ResetHideTimer();
    }
    ::InvalidateRect(hwnd_, nullptr, FALSE);
}

void MainWindow::LaunchLatest() {
    if (store_.latest().empty()) {
        Logger::Instance().Trace("no recent item to launch");
        return;
    }
    const size_t entry_index = store_.latest().front();
    HideLauncher();
    // The original does not bump Freq for the "last item" hotkey.
    LaunchEntry(entry_index, false);
}

void MainWindow::OnPaint() {
    PAINTSTRUCT ps = {};
    ::BeginPaint(hwnd_, &ps);

    if (d2d_.target() == nullptr && !d2d_.Initialize(hwnd_)) {
        ::EndPaint(hwnd_, &ps);
        return;
    }
    if (d2d_.BeginDraw()) {
        DrawFrame();
        d2d_.EndDraw();
    }
    ::EndPaint(hwnd_, &ps);
}

float MainWindow::MeasureWidth(const std::wstring& text,
                               IDWriteTextFormat* format) const {
    IDWriteFactory* write = D2DContext::WriteFactory();
    if (write == nullptr || format == nullptr || text.empty()) {
        return 0.0f;
    }
    IDWriteTextLayout* layout = nullptr;
    if (FAILED(write->CreateTextLayout(text.c_str(), static_cast<UINT32>(text.size()),
                                       format, 4096.0f, 64.0f, &layout))) {
        return 0.0f;
    }
    DWRITE_TEXT_METRICS metrics = {};
    layout->GetMetrics(&metrics);
    layout->Release();
    return metrics.widthIncludingTrailingWhitespace;
}

D2D1_RECT_F MainWindow::InputRect() const {
    const float top = kPad + kHeaderHeight;
    return D2D1::RectF(kPad + 8.0f, top, settings_.form_width - kPad - 8.0f,
                       top + kInputHeight);
}

D2D1_RECT_F MainWindow::RowRect(size_t row) const {
    const float top = kPad + kHeaderHeight + kInputHeight + row * kRowHeight;
    return D2D1::RectF(kPad, top, settings_.form_width - kPad, top + kRowHeight);
}

std::string MainWindow::NumberPrefixFor(size_t row, const ShortcutEntry& entry) const {
    if (!settings_.number_key) {
        return {};
    }
    return NumberPrefix(row, entry.item.HasParam(), settings_.index_from_0_to_9);
}

std::string MainWindow::FormatRow(size_t row) const {
    const ShortcutEntry& entry = store_.entries()[matches_[row].entry_index];
    std::string line = NumberPrefixFor(row, entry);
    line += FormatListLine(settings_.list_format, entry.item.keyword,
                           entry.item.DisplayName());
    return line;
}

void MainWindow::DrawFrame() {
    const float width = static_cast<float>(settings_.form_width);
    const float height = ContentHeight();
    const float radius = static_cast<float>(std::max(0, settings_.round_border_radius));

    d2d_.FillRoundedRect(D2D1::RectF(0.5f, 0.5f, width - 0.5f, height - 0.5f), radius,
                         kBackground);
    d2d_.DrawRoundedBorder(D2D1::RectF(0.5f, 0.5f, width - 0.5f, height - 0.5f), radius,
                           kBorder, 1.0f);

    d2d_.DrawTextBlock(L"ALTRun", title_format_.Get(),
                       D2D1::RectF(kPad, kPad, width - kPad, kPad + kHeaderHeight),
                       kAccent);
    const std::wstring count =
        std::to_wstring(store_.item_count()) + L" items";
    IDWriteTextFormat* right = title_format_.Get();
    right->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
    d2d_.DrawTextBlock(count, right,
                       D2D1::RectF(kPad, kPad, width - kPad, kPad + kHeaderHeight),
                       kTextDim);
    right->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);

    // Query line with caret.
    const D2D1_RECT_F input = InputRect();
    const std::wstring query_wide = ToWide(query_);
    if (query_wide.empty() && ime_composition_.empty()) {
        d2d_.DrawTextBlock(L"输入关键字，Enter 运行，Esc 关闭", hint_format_.Get(), input,
                           kTextDim);
    } else {
        d2d_.DrawTextBlock(query_wide, query_format_.Get(), input, kTextBright);
        if (caret_visible_) {
            const float caret_x =
                input.left + MeasureWidth(query_wide, query_format_.Get()) + 2.0f;
            d2d_.FillRect(D2D1::RectF(caret_x, input.top + 8.0f, caret_x + 1.5f,
                                      input.bottom - 8.0f),
                          kCaret);
        }
    }
    if (!ime_composition_.empty()) {
        const float offset = MeasureWidth(query_wide, query_format_.Get());
        d2d_.DrawTextBlock(ime_composition_, query_format_.Get(),
                           D2D1::RectF(input.left + offset, input.top, input.right,
                                       input.bottom),
                           kTextDim);
    }

    DrawRows();
    DrawFooter();
}

void MainWindow::DrawRows() {
    const size_t rows = VisibleRows();
    for (size_t row = 0; row < rows; ++row) {
        const D2D1_RECT_F rect = RowRect(row);
        const bool is_selected = static_cast<int>(row) == selected_;
        const bool is_hover = static_cast<int>(row) == hover_;

        if (is_selected) {
            const float radius = std::max(
                0.0f, static_cast<float>(settings_.round_border_radius) - 6.0f);
            d2d_.FillRoundedRect(
                D2D1::RectF(rect.left, rect.top + 1.0f, rect.right, rect.bottom - 1.0f),
                radius, kSelected);
        } else if (is_hover) {
            d2d_.FillRoundedRect(rect, 6.0f, kHover);
        }

        const std::string line = FormatRow(row);
        const std::wstring wide = ToWide(line);
        d2d_.DrawTextBlock(wide, mono_format_.Get(),
                           D2D1::RectF(rect.left + 10.0f, rect.top, rect.right - 8.0f,
                                       rect.bottom),
                           kTextBright);
    }

    if (rows == 0) {
        const D2D1_RECT_F rect = RowRect(0);
        d2d_.DrawTextBlock(L"没有匹配项", row_format_.Get(),
                           D2D1::RectF(rect.left + 10.0f, rect.top, rect.right,
                                       rect.bottom),
                           kTextDim);
    }
}

void MainWindow::DrawFooter() {
    const float rows_top = kPad + kHeaderHeight + kInputHeight;
    const float detail_top = rows_top + VisibleRows() * kRowHeight;
    const float hint_top = detail_top + kDetailHeight;

    // Selected item detail: name + command line (ShowCommandLine).
    if (settings_.show_command_line && !matches_.empty() && selected_ >= 0 &&
        selected_ < static_cast<int>(matches_.size())) {
        const ShortcutEntry& entry = store_.entries()[matches_[selected_].entry_index];
        const std::wstring detail =
            ToWide(entry.item.DisplayName() + "  " + entry.item.command_line);
        d2d_.DrawTextBlock(detail, hint_format_.Get(),
                           D2D1::RectF(kPad + 4.0f, detail_top,
                                       static_cast<float>(settings_.form_width) - kPad,
                                       detail_top + kDetailHeight),
                           kTextDim);
    }

    if (!settings_.show_operation_hint) {
        return;
    }
    d2d_.DrawTextBlock(L"↑↓ 选择   Enter 运行   Esc 关闭   Ctrl+V 粘贴",
                       hint_format_.Get(),
                       D2D1::RectF(kPad + 4.0f, hint_top,
                                   static_cast<float>(settings_.form_width) - kPad,
                                   hint_top + kFooterHeight),
                       kTextDim);
}

void MainWindow::MoveSelection(int delta) {
    if (matches_.empty()) {
        return;
    }
    int next = selected_ + delta;
    const int count = static_cast<int>(matches_.size());
    next = std::clamp(next, 0, count - 1);
    if (next != selected_) {
        selected_ = next;
        ::InvalidateRect(hwnd_, nullptr, FALSE);
    }
}

void MainWindow::LaunchRow(size_t row) {
    if (row >= matches_.size()) {
        return;
    }
    LaunchEntry(matches_[row].entry_index);
}

void MainWindow::LaunchEntry(size_t entry_index, bool count_freq) {
    const std::vector<ShortcutEntry>& entries = store_.entries();
    if (entry_index >= entries.size() || entries[entry_index].is_blank) {
        return;
    }
    const Shortcut item = entries[entry_index].item;

    if (NeedsParamPrompt(item) && param_window_) {
        // Prompt first, then launch with the confirmed parameter.
        param_window_->Open(item, [this, entry_index, count_freq](std::string param) {
            ExecuteEntry(entry_index, std::move(param), count_freq);
        });
        return;
    }
    ExecuteEntry(entry_index, "", count_freq);
}

void MainWindow::ExecuteEntry(size_t entry_index, std::string param, bool count_freq) {
    std::vector<ShortcutEntry>& entries = store_.mutable_entries();
    if (entry_index >= entries.size() || entries[entry_index].is_blank) {
        return;
    }
    const Shortcut& item = entries[entry_index].item;

    const LaunchResult result = LaunchShortcut(item, param);
    if (!result.ok) {
        ::MessageBeep(MB_ICONWARNING);
        return;
    }

    if (count_freq) {
        store_.TouchEntry(entry_index);
    }
    store_.RecordLatest(entry_index);
    store_.RememberFavourite(query_, item.DisplayName());
    store_.Save();

    HideLauncher();
    ClearQuery();
    showing_recent_ = false;

    if (settings_.exit_when_execute && exit_handler_) {
        exit_handler_();
    }
}

void MainWindow::AppendQuery(const std::wstring& text) {
    if (text.empty()) {
        return;
    }
    showing_recent_ = false;
    query_ += ToUtf8(text);
    caret_visible_ = true;
    RefreshMatches();
}

void MainWindow::Backspace() {
    if (query_.empty()) {
        return;
    }
    showing_recent_ = false;
    size_t index = query_.size() - 1;
    while (index > 0 && (static_cast<unsigned char>(query_[index]) & 0xC0) == 0x80) {
        --index;
    }
    query_.erase(index);
    caret_visible_ = true;
    RefreshMatches();
}

void MainWindow::PasteFromClipboard() {
    if (!::OpenClipboard(hwnd_)) {
        return;
    }
    HANDLE data = ::GetClipboardData(CF_UNICODETEXT);
    if (data != nullptr) {
        const auto* text = static_cast<const wchar_t*>(::GlobalLock(data));
        if (text != nullptr) {
            std::wstring pasted(text);
            ::GlobalUnlock(data);
            for (wchar_t& c : pasted) {
                if (c == L'\r' || c == L'\n') {
                    c = L' ';
                }
            }
            AppendQuery(pasted);
        }
    }
    ::CloseClipboard();
}

void MainWindow::HandleKeyDown(WPARAM vk) {
    ResetHideTimer();
    const bool ctrl = (::GetKeyState(VK_CONTROL) & 0x8000) != 0;
    switch (vk) {
        case VK_ESCAPE:
            HideLauncher();
            return;
        case VK_RETURN:
            LaunchRow(static_cast<size_t>(selected_));
            return;
        case VK_UP:
            MoveSelection(-1);
            return;
        case VK_DOWN:
            MoveSelection(1);
            return;
        case VK_PRIOR:
            MoveSelection(-static_cast<int>(kTopTenRows));
            return;
        case VK_NEXT:
            MoveSelection(static_cast<int>(kTopTenRows));
            return;
        case VK_BACK:
            Backspace();
            return;
        case VK_DELETE:
            if (ctrl) {
                ClearQuery();
                RefreshMatches();
            }
            return;
        default:
            break;
    }

    if (ctrl && vk == 'V') {
        PasteFromClipboard();
        return;
    }
    if (ctrl && vk == 'L') {
        ShowRecentList();
        return;
    }

    // Number-key launch (original NumberKey option).
    if (settings_.number_key && vk >= '0' && vk <= '9' && !ctrl) {
        const int base = settings_.index_from_0_to_9 ? 0 : 1;
        const int number = static_cast<int>(vk - '0');
        int index = number - base;
        if (!settings_.index_from_0_to_9 && number == 0) {
            index = 9;  // the 10th row is labelled "0"
        }
        if (index >= 0 && index < static_cast<int>(VisibleRows())) {
            LaunchRow(static_cast<size_t>(index));
        }
    }
}

void MainWindow::HandleChar(WPARAM ch) {
    const wchar_t c = static_cast<wchar_t>(ch);
    if (c < 0x20 || c == 0x7F) {
        return;
    }
    // Trailing space runs the current selection; ';' and '\'' run the 2nd and
    // 3rd visible items (original key handling, spec 3.8).
    if (c == L' ' && !query_.empty()) {
        LaunchRow(static_cast<size_t>(selected_));
        return;
    }
    if (c == L';') {
        LaunchRow(1);
        return;
    }
    if (c == L'\'') {
        LaunchRow(2);
        return;
    }
    if (settings_.number_key && c >= L'0' && c <= L'9') {
        return;  // consumed as a launch shortcut
    }
    AppendQuery(std::wstring(1, c));
}

void MainWindow::HandleImeComposition(LPARAM lparam) {
    HIMC context = ::ImmGetContext(hwnd_);
    if (context == nullptr) {
        return;
    }

    if ((lparam & GCS_RESULTSTR) != 0) {
        const LONG bytes = ::ImmGetCompositionStringW(context, GCS_RESULTSTR, nullptr, 0);
        if (bytes > 0) {
            std::wstring result(static_cast<size_t>(bytes / sizeof(wchar_t)), L'\0');
            ::ImmGetCompositionStringW(context, GCS_RESULTSTR, result.data(),
                                       static_cast<DWORD>(bytes));
            ime_composition_.clear();
            AppendQuery(result);
        }
    } else if ((lparam & GCS_COMPSTR) != 0) {
        const LONG bytes = ::ImmGetCompositionStringW(context, GCS_COMPSTR, nullptr, 0);
        if (bytes > 0) {
            ime_composition_.assign(static_cast<size_t>(bytes / sizeof(wchar_t)), L'\0');
            ::ImmGetCompositionStringW(context, GCS_COMPSTR, ime_composition_.data(),
                                       static_cast<DWORD>(bytes));
        } else {
            ime_composition_.clear();
        }
        ::InvalidateRect(hwnd_, nullptr, FALSE);
    }

    ::ImmReleaseContext(hwnd_, context);
}

void MainWindow::PositionImeWindow() {
    HIMC context = ::ImmGetContext(hwnd_);
    if (context == nullptr) {
        return;
    }
    const D2D1_RECT_F input = InputRect();
    const float x = input.left + MeasureWidth(ToWide(query_), query_format_.Get());

    COMPOSITIONFORM composition = {};
    composition.dwStyle = CFS_POINT;
    composition.ptCurrentPos.x = static_cast<LONG>(x);
    composition.ptCurrentPos.y = static_cast<LONG>(input.top + 4.0f);
    ::ImmSetCompositionWindow(context, &composition);

    ::ImmReleaseContext(hwnd_, context);
}

LRESULT MainWindow::HandleMessage(UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
        case WM_ACTIVATE:
            if (LOWORD(wparam) == WA_INACTIVE && visible() && !dragging_) {
                HideLauncher();
            }
            return 0;
        case WM_KEYDOWN:
            HandleKeyDown(wparam);
            return 0;
        case WM_CHAR:
            HandleChar(wparam);
            return 0;
        case WM_TIMER:
            if (wparam == kTimerCaret) {
                caret_visible_ = !caret_visible_;
                ::InvalidateRect(hwnd_, nullptr, FALSE);
            } else if (wparam == kTimerHide) {
                HideLauncher();
            }
            return 0;
        case kMessageAutoExecute:
            if (settings_.execute_if_only_one && matches_.size() == 1 &&
                !query_.empty()) {
                LaunchRow(0);
            }
            return 0;
        case WM_IME_STARTCOMPOSITION:
            PositionImeWindow();
            break;
        case WM_IME_COMPOSITION:
            HandleImeComposition(lparam);
            return 0;
        case WM_MOUSEMOVE: {
            const POINT pt = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
            ResetHideTimer();
            if (dragging_) {
                POINT cursor = {};
                ::GetCursorPos(&cursor);
                const POINT delta = {cursor.x - drag_origin_.x, cursor.y - drag_origin_.y};
                RECT rect = {};
                ::GetWindowRect(hwnd_, &rect);
                ::SetWindowPos(hwnd_, HWND_TOPMOST, rect.left + delta.x,
                               rect.top + delta.y, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE);
                drag_origin_ = cursor;
                return 0;
            }
            int hover = -1;
            for (size_t row = 0; row < VisibleRows(); ++row) {
                const D2D1_RECT_F rect = RowRect(row);
                if (pt.x >= rect.left && pt.x <= rect.right && pt.y >= rect.top &&
                    pt.y <= rect.bottom) {
                    hover = static_cast<int>(row);
                    break;
                }
            }
            if (hover != hover_) {
                hover_ = hover;
                ::InvalidateRect(hwnd_, nullptr, FALSE);
            }
            if (pt.y < kPad + kHeaderHeight) {
                ::SetCursor(::LoadCursorW(nullptr, IDC_SIZEALL));
            }
            return 0;
        }
        case WM_LBUTTONDOWN: {
            const POINT pt = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
            if (pt.y < kPad + kHeaderHeight) {
                dragging_ = true;
                ::GetCursorPos(&drag_origin_);
                ::SetCapture(hwnd_);
                return 0;
            }
            for (size_t row = 0; row < VisibleRows(); ++row) {
                const D2D1_RECT_F rect = RowRect(row);
                if (pt.x >= rect.left && pt.x <= rect.right && pt.y >= rect.top &&
                    pt.y <= rect.bottom) {
                    selected_ = static_cast<int>(row);
                    LaunchRow(row);
                    return 0;
                }
            }
            return 0;
        }
        case WM_LBUTTONUP:
            if (dragging_) {
                dragging_ = false;
                ::ReleaseCapture();
                return 0;
            }
            break;
        case WM_MOUSELEAVE:
            if (hover_ != -1) {
                hover_ = -1;
                ::InvalidateRect(hwnd_, nullptr, FALSE);
            }
            return 0;
        case WM_DESTROY:
            ::KillTimer(hwnd_, kTimerCaret);
            ::KillTimer(hwnd_, kTimerHide);
            return 0;
        default:
            break;
    }
    return Window::HandleMessage(message, wparam, lparam);
}

void MainWindow::SetExitHandler(std::function<void()> handler) {
    exit_handler_ = std::move(handler);
}

}  // namespace alt
