#include "ui/ParamWindow.h"

#include <commctrl.h>
#include <dwmapi.h>
#include <windowsx.h>

#include <algorithm>

#include "core/Logger.h"
#include "core/Text.h"

namespace alt {
namespace {

constexpr float kPad = 12.0f;
constexpr float kTitleHeight = 20.0f;
constexpr float kEditHeight = 28.0f;
constexpr float kRowHeight = 22.0f;
constexpr float kFooterHeight = 18.0f;
constexpr float kOkWidth = 54.0f;
constexpr UINT kTimerIdle = 3;
constexpr size_t kMaxHistoryRows = 6;

const D2D1_COLOR_F kBackground = D2D1::ColorF(0.102f, 0.110f, 0.125f, 1.0f);
const D2D1_COLOR_F kBorder = D2D1::ColorF(0.239f, 0.255f, 0.290f, 1.0f);
const D2D1_COLOR_F kEditBackground = D2D1::ColorF(0.059f, 0.067f, 0.078f, 1.0f);
const D2D1_COLOR_F kSelected = D2D1::ColorF(0.161f, 0.290f, 0.435f, 1.0f);
const D2D1_COLOR_F kTextBright = D2D1::ColorF(0.925f, 0.937f, 0.957f, 1.0f);
const D2D1_COLOR_F kTextDim = D2D1::ColorF(0.541f, 0.569f, 0.612f, 1.0f);
const D2D1_COLOR_F kAccent = D2D1::ColorF(0.361f, 0.659f, 0.988f, 1.0f);

constexpr int kDefaultWidth = 460;

}  // namespace

ParamWindow::ParamWindow(Settings& settings, ParamHistory& history)
    : settings_(settings), history_(history) {}

ParamWindow::~ParamWindow() {
    if (edit_brush_ != nullptr) {
        ::DeleteObject(edit_brush_);
        edit_brush_ = nullptr;
    }
}

bool ParamWindow::CreateParamWindow(HINSTANCE instance) {
    return Create(instance, L"ALTRun.ParamWindow.v2", L"ALTRun 参数",
                  WS_POPUP, WS_EX_TOPMOST | WS_EX_TOOLWINDOW);
}

void ParamWindow::OnCreate() {
    title_format_.Attach(d2d_.CreateFormat(12.0f, DWRITE_FONT_WEIGHT_SEMI_BOLD));
    row_format_.Attach(d2d_.CreateFormat(13.0f, DWRITE_FONT_WEIGHT_NORMAL));
    hint_format_.Attach(d2d_.CreateFormat(11.0f, DWRITE_FONT_WEIGHT_NORMAL));

    edit_brush_ = ::CreateSolidBrush(RGB(15, 17, 20));

    edit_ = ::CreateWindowExW(0, L"EDIT", L"",
                              WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_LEFT,
                              0, 0, 100, 26, hwnd_,
                              reinterpret_cast<HMENU>(1001), instance_, nullptr);
    if (edit_ != nullptr) {
        ::SendMessageW(edit_, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN,
                       MAKELPARAM(6, 6));
        ::SendMessageW(edit_, WM_SETFONT,
                       reinterpret_cast<WPARAM>(::GetStockObject(DEFAULT_GUI_FONT)),
                       TRUE);
        ::SetWindowSubclass(edit_, &ParamWindow::EditSubclassProc, 1,
                            reinterpret_cast<DWORD_PTR>(this));
    }

    d2d_.Initialize(hwnd_);
    const DWORD corner_pref = 2;
    ::DwmSetWindowAttribute(hwnd_, 33, &corner_pref, sizeof(corner_pref));
}

void ParamWindow::Open(const Shortcut& item, ConfirmHandler on_confirm) {
    if (hwnd_ == nullptr) {
        return;
    }
    item_label_ = ToWide(item.DisplayName() + "   输入参数");
    on_confirm_ = std::move(on_confirm);

    shown_.clear();
    const size_t limit = std::min(kMaxHistoryRows, history_.items().size());
    for (size_t i = 0; i < limit; ++i) {
        shown_.push_back(history_.items()[i]);
    }
    selected_ = -1;
    SetEditText(L"");

    PlaceOnCursorMonitor();
    Relayout();
    ::ShowWindow(hwnd_, SW_SHOWNORMAL);
    ::SetForegroundWindow(hwnd_);
    if (edit_ != nullptr) {
        ::SetFocus(edit_);
    }
    ResetIdleTimer();
    ::InvalidateRect(hwnd_, nullptr, FALSE);
    Logger::Instance().Trace("param window opened for " +
                             std::string(item.DisplayName()));
}

size_t ParamWindow::VisibleHistoryRows() const {
    return std::min(shown_.size(), kMaxHistoryRows);
}

D2D1_RECT_F ParamWindow::EditRect() const {
    const float top = kPad + kTitleHeight;
    return D2D1::RectF(kPad, top, kDefaultWidth - kPad - kOkWidth - 8.0f,
                       top + kEditHeight);
}

D2D1_RECT_F ParamWindow::OkButtonRect() const {
    const float top = kPad + kTitleHeight;
    return D2D1::RectF(kDefaultWidth - kPad - kOkWidth, top,
                       kDefaultWidth - kPad, top + kEditHeight);
}

D2D1_RECT_F ParamWindow::HistoryRowRect(size_t row) const {
    const float top = kPad + kTitleHeight + kEditHeight + 6.0f + row * kRowHeight;
    return D2D1::RectF(kPad, top, kDefaultWidth - kPad, top + kRowHeight);
}

void ParamWindow::Relayout() {
    const int width = kDefaultWidth;
    const int height = static_cast<int>(kPad + kTitleHeight + kEditHeight + 6.0f +
                                        VisibleHistoryRows() * kRowHeight +
                                        kFooterHeight + kPad);
    ::SetWindowPos(hwnd_, HWND_TOPMOST, 0, 0, width, height,
                   SWP_NOMOVE | SWP_NOACTIVATE);
    d2d_.Resize(static_cast<UINT>(width), static_cast<UINT>(height));

    if (edit_ != nullptr) {
        const D2D1_RECT_F rect = EditRect();
        ::MoveWindow(edit_, static_cast<int>(rect.left), static_cast<int>(rect.top),
                     static_cast<int>(rect.right - rect.left),
                     static_cast<int>(rect.bottom - rect.top), TRUE);
    }
}

void ParamWindow::PlaceOnCursorMonitor() {
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
    const int x = mi.rcWork.left + (mi.rcWork.right - mi.rcWork.left - width) / 2;
    const int y = mi.rcWork.top + (mi.rcWork.bottom - mi.rcWork.top - height) / 2;
    ::SetWindowPos(hwnd_, HWND_TOPMOST, x, y, 0, 0,
                   SWP_NOSIZE | SWP_NOACTIVATE);
}

void ParamWindow::ResetIdleTimer() {
    ::KillTimer(hwnd_, kTimerIdle);
    if (settings_.hide_delay > 0) {
        ::SetTimer(hwnd_, kTimerIdle,
                   static_cast<UINT>(settings_.hide_delay) * 1000, nullptr);
    }
}

void ParamWindow::Cancel() {
    ::KillTimer(hwnd_, kTimerIdle);
    on_confirm_ = nullptr;
    ::ShowWindow(hwnd_, SW_HIDE);
}

std::wstring ParamWindow::EditText() const {
    if (edit_ == nullptr) {
        return {};
    }
    const int length = ::GetWindowTextLengthW(edit_);
    std::wstring text(static_cast<size_t>(length), L'\0');
    if (length > 0) {
        ::GetWindowTextW(edit_, text.data(), length + 1);
    }
    return text;
}

void ParamWindow::SetEditText(const std::wstring& text) {
    if (edit_ != nullptr) {
        ::SetWindowTextW(edit_, text.c_str());
        ::SendMessageW(edit_, EM_SETSEL, 0, -1);
    }
}

void ParamWindow::Confirm() {
    ::KillTimer(hwnd_, kTimerIdle);
    const std::wstring text = EditText();
    if (!text.empty()) {
        history_.Record(ToUtf8(text), settings_.param_history_limit);
    }
    ConfirmHandler handler = std::move(on_confirm_);
    on_confirm_ = nullptr;
    ::ShowWindow(hwnd_, SW_HIDE);
    if (handler) {
        handler(ToUtf8(text));
    }
}

void ParamWindow::HandleEscape() {
    // Two stage: clear the text first, cancel only when already empty.
    if (!EditText().empty()) {
        SetEditText(L"");
        selected_ = -1;
        ::InvalidateRect(hwnd_, nullptr, FALSE);
        return;
    }
    Cancel();
}

void ParamWindow::MoveHistory(int delta) {
    if (shown_.empty()) {
        return;
    }
    int next = selected_ + delta;
    next = std::clamp(next, -1, static_cast<int>(shown_.size()) - 1);
    if (next == selected_) {
        return;
    }
    selected_ = next;
    ApplyHistorySelection();
    ::InvalidateRect(hwnd_, nullptr, FALSE);
}

void ParamWindow::ApplyHistorySelection() {
    if (selected_ >= 0 && selected_ < static_cast<int>(shown_.size())) {
        SetEditText(ToWide(shown_[selected_].text));
    } else {
        SetEditText(L"");
    }
}

void ParamWindow::OnPaint() {
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

void ParamWindow::DrawFrame() {
    RECT client = {};
    ::GetClientRect(hwnd_, &client);
    const float width = static_cast<float>(client.right);
    const float height = static_cast<float>(client.bottom);
    const float radius = static_cast<float>(std::max(0, settings_.round_border_radius));

    d2d_.FillRoundedRect(D2D1::RectF(0.5f, 0.5f, width - 0.5f, height - 0.5f), radius,
                         kBackground);
    d2d_.DrawRoundedBorder(D2D1::RectF(0.5f, 0.5f, width - 0.5f, height - 0.5f), radius,
                           kBorder, 1.0f);

    d2d_.DrawTextBlock(item_label_, title_format_.Get(),
                       D2D1::RectF(kPad, kPad, width - kPad, kPad + kTitleHeight),
                       kAccent);

    // Edit background (the native EDIT control sits on top of it).
    const D2D1_RECT_F edit = EditRect();
    d2d_.FillRoundedRect(D2D1::RectF(edit.left, edit.top, edit.right, edit.bottom),
                         4.0f, kEditBackground);
    d2d_.DrawRoundedBorder(D2D1::RectF(edit.left, edit.top, edit.right, edit.bottom),
                           4.0f, kBorder, 1.0f);

    // OK button.
    const D2D1_RECT_F ok = OkButtonRect();
    d2d_.FillRoundedRect(ok, 4.0f, kSelected);
    IDWriteTextFormat* centered = hint_format_.Get();
    centered->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    d2d_.DrawTextBlock(L"确定", centered, ok, kTextBright);
    centered->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);

    DrawHistory();

    const float footer_top = kPad + kTitleHeight + kEditHeight + 6.0f +
                             VisibleHistoryRows() * kRowHeight;
    d2d_.DrawTextBlock(L"Enter 确定    Esc 清空/取消    空闲自动关闭", hint_format_.Get(),
                       D2D1::RectF(kPad, footer_top, width - kPad,
                                   footer_top + kFooterHeight),
                       kTextDim);
}

void ParamWindow::DrawHistory() {
    for (size_t row = 0; row < VisibleHistoryRows(); ++row) {
        const D2D1_RECT_F rect = HistoryRowRect(row);
        if (static_cast<int>(row) == selected_) {
            d2d_.FillRoundedRect(rect, 4.0f, kSelected);
        }
        const ParamHistoryItem& item = shown_[row];
        const std::wstring text = ToWide(item.text);
        const std::wstring rank = std::to_wstring(item.rank) + L"x";
        d2d_.DrawTextBlock(rank, hint_format_.Get(),
                           D2D1::RectF(rect.left + 4.0f, rect.top,
                                       rect.left + 40.0f, rect.bottom),
                           kTextDim);
        d2d_.DrawTextBlock(text, row_format_.Get(),
                           D2D1::RectF(rect.left + 42.0f, rect.top, rect.right,
                                       rect.bottom),
                           kTextBright);
    }
}

LRESULT CALLBACK ParamWindow::EditSubclassProc(HWND hwnd, UINT message, WPARAM wparam,
                                               LPARAM lparam, UINT_PTR /*subclass_id*/,
                                               DWORD_PTR ref_data) {
    auto* self = reinterpret_cast<ParamWindow*>(ref_data);
    if (self == nullptr) {
        return ::DefSubclassProc(hwnd, message, wparam, lparam);
    }
    if (message == WM_KEYDOWN) {
        self->ResetIdleTimer();
        switch (wparam) {
            case VK_RETURN:
                self->Confirm();
                return 0;
            case VK_ESCAPE:
                self->HandleEscape();
                return 0;
            case VK_UP:
                self->MoveHistory(-1);
                return 0;
            case VK_DOWN:
                self->MoveHistory(1);
                return 0;
            default:
                break;
        }
    } else if (message == WM_CHAR) {
        self->ResetIdleTimer();
    }
    return ::DefSubclassProc(hwnd, message, wparam, lparam);
}

LRESULT ParamWindow::HandleMessage(UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
        case WM_CTLCOLOREDIT: {
            HDC dc = reinterpret_cast<HDC>(wparam);
            ::SetBkColor(dc, RGB(15, 17, 20));
            ::SetTextColor(dc, RGB(236, 239, 244));
            return reinterpret_cast<LRESULT>(edit_brush_);
        }
        case WM_ACTIVATE:
            ResetIdleTimer();
            return 0;
        case WM_KEYDOWN:
            ResetIdleTimer();
            switch (wparam) {
                case VK_RETURN:
                    Confirm();
                    return 0;
                case VK_ESCAPE:
                    HandleEscape();
                    return 0;
                case VK_UP:
                    MoveHistory(-1);
                    return 0;
                case VK_DOWN:
                    MoveHistory(1);
                    return 0;
                default:
                    break;
            }
            break;
        case WM_LBUTTONDOWN: {
            ResetIdleTimer();
            const POINT pt = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
            const D2D1_RECT_F ok = OkButtonRect();
            if (pt.x >= ok.left && pt.x <= ok.right && pt.y >= ok.top &&
                pt.y <= ok.bottom) {
                Confirm();
                return 0;
            }
            for (size_t row = 0; row < VisibleHistoryRows(); ++row) {
                const D2D1_RECT_F rect = HistoryRowRect(row);
                if (pt.x >= rect.left && pt.x <= rect.right && pt.y >= rect.top &&
                    pt.y <= rect.bottom) {
                    selected_ = static_cast<int>(row);
                    ApplyHistorySelection();
                    ::InvalidateRect(hwnd_, nullptr, FALSE);
                    if (edit_ != nullptr) {
                        ::SetFocus(edit_);
                    }
                    return 0;
                }
            }
            return 0;
        }
        case WM_TIMER:
            if (wparam == kTimerIdle) {
                Cancel();
            }
            return 0;
        case WM_DESTROY:
            ::KillTimer(hwnd_, kTimerIdle);
            if (edit_ != nullptr) {
                ::RemoveWindowSubclass(edit_, &ParamWindow::EditSubclassProc, 1);
            }
            return 0;
        default:
            break;
    }
    return Window::HandleMessage(message, wparam, lparam);
}

}  // namespace alt
