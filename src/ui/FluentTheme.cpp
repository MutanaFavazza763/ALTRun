#include "ui/FluentTheme.h"

#include <commctrl.h>
#include <objidl.h>
#include <rpcndr.h>
#include <oaidl.h>
#include <propidl.h>
#include <combaseapi.h>
#include <gdiplus.h>
#include <windowsx.h>

#include <algorithm>
#include <iterator>
#include <memory>

#include "resource_ids.h"

namespace alt {
namespace fluent {
namespace {

constexpr wchar_t kCloseHotProp[] = L"ALTRunFluentCloseHot";

ULONG_PTR GdiplusToken() {
    static ULONG_PTR token = 0;
    static bool started = false;
    if (!started) {
        Gdiplus::GdiplusStartupInput input;
        Gdiplus::GdiplusStartup(&token, &input, nullptr);
        started = true;
    }
    return token;
}

Gdiplus::Color ToColor(COLORREF color, BYTE alpha = 255) {
    return Gdiplus::Color(alpha, GetRValue(color), GetGValue(color),
                          GetBValue(color));
}

void AddRoundedRect(Gdiplus::GraphicsPath& path, const Gdiplus::RectF& rect,
                    float radius) {
    const float diameter = std::min(radius * 2.0f,
                                    std::min(rect.Width, rect.Height));
    if (diameter <= 1.0f) {
        path.AddRectangle(rect);
        return;
    }
    path.AddArc(rect.X, rect.Y, diameter, diameter, 180.0f, 90.0f);
    path.AddArc(rect.X + rect.Width - diameter, rect.Y, diameter, diameter,
                270.0f, 90.0f);
    path.AddArc(rect.X + rect.Width - diameter,
                rect.Y + rect.Height - diameter, diameter, diameter, 0.0f,
                90.0f);
    path.AddArc(rect.X, rect.Y + rect.Height - diameter, diameter, diameter,
                90.0f, 90.0f);
    path.CloseFigure();
}

void FillRoundedArgb(HDC dc, const RECT& rect, int radius, COLORREF color,
                     BYTE alpha) {
    GdiplusToken();
    Gdiplus::Graphics graphics(dc);
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    Gdiplus::GraphicsPath path;
    AddRoundedRect(path,
                   Gdiplus::RectF(static_cast<float>(rect.left),
                                  static_cast<float>(rect.top),
                                  static_cast<float>(rect.right - rect.left),
                                  static_cast<float>(rect.bottom - rect.top)),
                   static_cast<float>(radius));
    Gdiplus::SolidBrush brush(ToColor(color, alpha));
    graphics.FillPath(&brush, &path);
}

void StrokeRoundedArgb(HDC dc, const RECT& rect, int radius, COLORREF color,
                       BYTE alpha, float width) {
    GdiplusToken();
    Gdiplus::Graphics graphics(dc);
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    Gdiplus::GraphicsPath path;
    AddRoundedRect(path,
                   Gdiplus::RectF(static_cast<float>(rect.left),
                                  static_cast<float>(rect.top),
                                  static_cast<float>(rect.right - rect.left),
                                  static_cast<float>(rect.bottom - rect.top)),
                   static_cast<float>(radius));
    Gdiplus::Pen pen(ToColor(color, alpha), width);
    graphics.DrawPath(&pen, &path);
}

RECT InsetBy(const RECT& rect, int amount) {
    return RECT{rect.left + amount, rect.top + amount, rect.right - amount,
                rect.bottom - amount};
}

bool PointInRect(const RECT& rect, POINT point) {
    return point.x >= rect.left && point.x < rect.right && point.y >= rect.top &&
           point.y < rect.bottom;
}

void SetCloseHot(HWND window, bool hot) {
    const bool current = ::GetPropW(window, kCloseHotProp) != nullptr;
    if (current == hot) {
        return;
    }
    if (hot) {
        ::SetPropW(window, kCloseHotProp, reinterpret_cast<HANDLE>(1));
    } else {
        ::RemovePropW(window, kCloseHotProp);
    }
    ::InvalidateRect(window, nullptr, FALSE);
}

bool IsCloseHot(HWND window) {
    return ::GetPropW(window, kCloseHotProp) != nullptr;
}

void DrawCloseGlyph(HDC dc, const RECT& rect, COLORREF color) {
    const int cx = (rect.left + rect.right) / 2;
    const int cy = (rect.top + rect.bottom) / 2;
    const int arm = std::max<int>(5, (rect.right - rect.left) / 8);
    HPEN pen = ::CreatePen(PS_SOLID, 1, color);
    HGDIOBJ old = ::SelectObject(dc, pen);
    ::MoveToEx(dc, cx - arm, cy - arm, nullptr);
    ::LineTo(dc, cx + arm, cy + arm);
    ::MoveToEx(dc, cx + arm, cy - arm, nullptr);
    ::LineTo(dc, cx - arm, cy + arm);
    ::SelectObject(dc, old);
    ::DeleteObject(pen);
}

struct ToggleState {
    bool checked = false;
    bool hover = false;
    bool pressed = false;
    bool focused = false;
    HFONT font = nullptr;
};

struct ButtonState {
    bool hover = false;
    bool pressed = false;
    bool focused = false;
    bool primary = false;
    HFONT font = nullptr;
};

struct RadioState {
    bool checked = false;
    bool hover = false;
    bool focused = false;
    HFONT font = nullptr;
};

void NotifyParentClick(HWND control) {
    HWND parent = ::GetParent(control);
    if (parent != nullptr) {
        ::SendMessageW(parent, WM_COMMAND,
                       MAKEWPARAM(::GetDlgCtrlID(control), BN_CLICKED),
                       reinterpret_cast<LPARAM>(control));
    }
}

LRESULT CALLBACK ToggleProc(HWND hwnd, UINT message, WPARAM wparam,
                            LPARAM lparam, UINT_PTR, DWORD_PTR) {
    auto* state = reinterpret_cast<ToggleState*>(
        ::GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (state == nullptr) {
        return ::DefSubclassProc(hwnd, message, wparam, lparam);
    }

    switch (message) {
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT: {
            PAINTSTRUCT paint = {};
            HDC dc = ::BeginPaint(hwnd, &paint);
            RECT client = {};
            ::GetClientRect(hwnd, &client);
            ::FillRect(dc, &client, ::GetSysColorBrush(COLOR_WINDOW));
            wchar_t text[512] = {};
            ::GetWindowTextW(hwnd, text, static_cast<int>(std::size(text)));
            DrawToggle(dc, client, text, state->font, state->checked,
                       state->hover, state->focused);
            ::EndPaint(hwnd, &paint);
            return 0;
        }
        case WM_SETFONT:
            state->font = reinterpret_cast<HFONT>(wparam);
            ::InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        case WM_SETFOCUS:
            state->focused = true;
            ::InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        case WM_KILLFOCUS:
            state->focused = false;
            state->pressed = false;
            ::InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        case WM_ENABLE:
            state->pressed = false;
            state->hover = false;
            ::InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        case WM_MOUSEMOVE:
            if (!state->hover) {
                state->hover = true;
                TRACKMOUSEEVENT track = {sizeof(TRACKMOUSEEVENT), TME_LEAVE, hwnd,
                                         0};
                ::TrackMouseEvent(&track);
                ::InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        case WM_MOUSELEAVE:
            state->hover = false;
            state->pressed = false;
            ::InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        case WM_LBUTTONDOWN:
            state->pressed = true;
            ::SetCapture(hwnd);
            ::SetFocus(hwnd);
            ::InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        case WM_LBUTTONUP: {
            const bool was_pressed = state->pressed;
            state->pressed = false;
            if (::GetCapture() == hwnd) {
                ::ReleaseCapture();
            }
            RECT client = {};
            ::GetClientRect(hwnd, &client);
            POINT point = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
            if (was_pressed && PointInRect(client, point)) {
                state->checked = !state->checked;
                NotifyParentClick(hwnd);
            }
            ::InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        case WM_KEYDOWN: {
            const UINT key = static_cast<UINT>(wparam);
            if (key == VK_SPACE || key == VK_RETURN) {
                state->checked = !state->checked;
                NotifyParentClick(hwnd);
                ::InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
            break;
        }
        case WM_CHAR:
            if (wparam == L' ') {
                return 0;
            }
            break;
        case BM_SETCHECK:
            state->checked = wparam == BST_CHECKED;
            ::InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        case BM_GETCHECK:
            return state->checked ? BST_CHECKED : BST_UNCHECKED;
        case WM_NCDESTROY:
            ::RemoveWindowSubclass(hwnd, &ToggleProc, 1);
            delete state;
            ::SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
            break;
        default:
            break;
    }
    return ::DefSubclassProc(hwnd, message, wparam, lparam);
}

LRESULT CALLBACK ButtonProc(HWND hwnd, UINT message, WPARAM wparam,
                            LPARAM lparam, UINT_PTR, DWORD_PTR) {
    auto* state = reinterpret_cast<ButtonState*>(
        ::GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (state == nullptr) {
        return ::DefSubclassProc(hwnd, message, wparam, lparam);
    }

    switch (message) {
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT: {
            PAINTSTRUCT paint = {};
            HDC dc = ::BeginPaint(hwnd, &paint);
            RECT client = {};
            ::GetClientRect(hwnd, &client);
            ::FillRect(dc, &client, ::GetSysColorBrush(COLOR_WINDOW));
            wchar_t text[256] = {};
            ::GetWindowTextW(hwnd, text, static_cast<int>(std::size(text)));
            DrawButton(dc, client, text, state->font, state->primary,
                       state->hover, state->pressed, state->focused,
                       ::IsWindowEnabled(hwnd) != FALSE);
            ::EndPaint(hwnd, &paint);
            return 0;
        }
        case WM_SETFONT:
            state->font = reinterpret_cast<HFONT>(wparam);
            ::InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        case WM_SETFOCUS:
            state->focused = true;
            ::InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        case WM_KILLFOCUS:
            state->focused = false;
            state->pressed = false;
            ::InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        case WM_ENABLE:
            state->pressed = false;
            state->hover = false;
            ::InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        case WM_MOUSEMOVE:
            if (!state->hover) {
                state->hover = true;
                TRACKMOUSEEVENT track = {sizeof(TRACKMOUSEEVENT), TME_LEAVE, hwnd,
                                         0};
                ::TrackMouseEvent(&track);
                ::InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        case WM_MOUSELEAVE:
            state->hover = false;
            state->pressed = false;
            ::InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        case WM_LBUTTONDOWN:
            state->pressed = true;
            ::SetCapture(hwnd);
            ::SetFocus(hwnd);
            ::InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        case WM_LBUTTONUP: {
            const bool was_pressed = state->pressed;
            state->pressed = false;
            if (::GetCapture() == hwnd) {
                ::ReleaseCapture();
            }
            RECT client = {};
            ::GetClientRect(hwnd, &client);
            POINT point = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
            if (was_pressed && PointInRect(client, point)) {
                NotifyParentClick(hwnd);
            }
            ::InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        case WM_KEYDOWN:
            if (wparam == VK_SPACE || wparam == VK_RETURN) {
                NotifyParentClick(hwnd);
                return 0;
            }
            break;
        case WM_CHAR:
            if (wparam == L' ') {
                return 0;
            }
            break;
        case BM_CLICK:
            NotifyParentClick(hwnd);
            return 0;
        case WM_NCDESTROY:
            ::RemoveWindowSubclass(hwnd, &ButtonProc, 1);
            delete state;
            ::SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
            break;
        default:
            break;
    }
    return ::DefSubclassProc(hwnd, message, wparam, lparam);
}

LRESULT CALLBACK RadioProc(HWND hwnd, UINT message, WPARAM wparam,
                           LPARAM lparam, UINT_PTR, DWORD_PTR) {
    auto* state = reinterpret_cast<RadioState*>(
        ::GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (state == nullptr) {
        return ::DefSubclassProc(hwnd, message, wparam, lparam);
    }

    switch (message) {
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT: {
            PAINTSTRUCT paint = {};
            HDC dc = ::BeginPaint(hwnd, &paint);
            RECT client = {};
            ::GetClientRect(hwnd, &client);
            ::FillRect(dc, &client, ::GetSysColorBrush(COLOR_WINDOW));
            wchar_t text[512] = {};
            ::GetWindowTextW(hwnd, text, static_cast<int>(std::size(text)));
            DrawRadio(dc, client, text, state->font, state->checked, state->hover,
                      state->focused);
            ::EndPaint(hwnd, &paint);
            return 0;
        }
        case WM_SETFONT:
            state->font = reinterpret_cast<HFONT>(wparam);
            ::InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        case WM_SETFOCUS:
            state->focused = true;
            ::InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        case WM_KILLFOCUS:
            state->focused = false;
            ::InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        case WM_MOUSEMOVE:
            if (!state->hover) {
                state->hover = true;
                TRACKMOUSEEVENT track = {sizeof(TRACKMOUSEEVENT), TME_LEAVE, hwnd,
                                         0};
                ::TrackMouseEvent(&track);
                ::InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        case WM_MOUSELEAVE:
            state->hover = false;
            ::InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        case WM_LBUTTONUP: {
            RECT client = {};
            ::GetClientRect(hwnd, &client);
            POINT point = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
            if (PointInRect(client, point)) {
                state->checked = true;
                NotifyParentClick(hwnd);
                ::InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        }
        case WM_KEYDOWN:
            if (wparam == VK_SPACE || wparam == VK_RETURN) {
                state->checked = true;
                NotifyParentClick(hwnd);
                ::InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
            break;
        case WM_CHAR:
            if (wparam == L' ') {
                return 0;
            }
            break;
        case BM_SETCHECK:
            state->checked = wparam == BST_CHECKED;
            ::InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        case BM_GETCHECK:
            return state->checked ? BST_CHECKED : BST_UNCHECKED;
        case WM_NCDESTROY:
            ::RemoveWindowSubclass(hwnd, &RadioProc, 1);
            delete state;
            ::SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
            break;
        default:
            break;
    }
    return ::DefSubclassProc(hwnd, message, wparam, lparam);
}

HWND CreateCustomButton(HWND parent, HINSTANCE instance, int id,
                        const std::wstring& text, HFONT font, int width,
                        int height, int dpi, bool primary, bool radio) {
    HWND control = ::CreateWindowExW(
        0, WC_BUTTONW, text.c_str(),
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW, 0, 0,
        ::MulDiv(width, dpi, USER_DEFAULT_SCREEN_DPI),
        ::MulDiv(height, dpi, USER_DEFAULT_SCREEN_DPI), parent,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), instance, nullptr);
    if (control == nullptr) {
        return nullptr;
    }
    if (font != nullptr) {
        ::SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    }
    if (radio) {
        auto* state = new RadioState();
        state->font = font;
        ::SetWindowLongPtrW(control, GWLP_USERDATA,
                            reinterpret_cast<LONG_PTR>(state));
        ::SetWindowSubclass(control, &RadioProc, 1, 0);
    } else if (primary) {
        auto* state = new ButtonState();
        state->font = font;
        state->primary = true;
        ::SetWindowLongPtrW(control, GWLP_USERDATA,
                            reinterpret_cast<LONG_PTR>(state));
        ::SetWindowSubclass(control, &ButtonProc, 1, 0);
    } else if (id > 0) {
        // Buttons are also used for secondary command buttons; id == 0 is
        // reserved for radio controls by the callers below.
        auto* state = new ButtonState();
        state->font = font;
        state->primary = false;
        ::SetWindowLongPtrW(control, GWLP_USERDATA,
                            reinterpret_cast<LONG_PTR>(state));
        ::SetWindowSubclass(control, &ButtonProc, 1, 0);
    }
    return control;
}

}  // namespace

int Scale(HWND window, int value) {
    const int dpi = static_cast<int>(::GetDpiForWindow(window));
    return ::MulDiv(value, dpi <= 0 ? USER_DEFAULT_SCREEN_DPI : dpi,
                    USER_DEFAULT_SCREEN_DPI);
}

HFONT CreateFont(HWND window, int point_size, bool semibold) {
    NONCLIENTMETRICSW metrics = {};
    metrics.cbSize = sizeof(metrics);
    LOGFONTW log_font = {};
    if (::SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(metrics),
                                &metrics, 0) != FALSE) {
        log_font = metrics.lfMessageFont;
    } else {
        log_font.lfHeight = -12;
        log_font.lfWeight = FW_NORMAL;
        wcscpy_s(log_font.lfFaceName, L"Segoe UI");
    }
    const int dpi = static_cast<int>(::GetDpiForWindow(window));
    log_font.lfHeight = -::MulDiv(point_size, dpi <= 0 ? 96 : dpi, 72);
    log_font.lfWeight = semibold ? FW_SEMIBOLD : FW_NORMAL;
    return ::CreateFontIndirectW(&log_font);
}

HICON AppIcon() {
    static HICON icon = ::LoadIconW(::GetModuleHandleW(nullptr),
                                    MAKEINTRESOURCEW(IDI_ALTRUN));
    return icon != nullptr ? icon : ::LoadIconW(nullptr, IDI_APPLICATION);
}

void FillBackground(HDC dc, const RECT& client) {
    GdiplusToken();
    Gdiplus::Graphics graphics(dc);
    Gdiplus::Rect rect(client.left, client.top, client.right - client.left,
                       client.bottom - client.top);
    if (rect.Width <= 0 || rect.Height <= 0) {
        return;
    }
    Gdiplus::LinearGradientBrush brush(
        rect, ToColor(kBackgroundTop), ToColor(kBackgroundBottom),
        Gdiplus::LinearGradientModeVertical);
    graphics.FillRectangle(&brush, rect);
}

void FillRounded(HDC dc, const RECT& rect, int radius, COLORREF color) {
    FillRoundedArgb(dc, rect, radius, color, 255);
}

void StrokeRounded(HDC dc, const RECT& rect, int radius, COLORREF color,
                   int width) {
    StrokeRoundedArgb(dc, rect, radius, color, 255, static_cast<float>(width));
}

void DrawCard(HDC dc, const RECT& rect, int radius) {
    RECT shadow = rect;
    ::OffsetRect(&shadow, 0, 3);
    FillRoundedArgb(dc, shadow, radius, kShadow, 40);
    FillRounded(dc, rect, radius, kSurface);
    RECT inner = InsetBy(rect, 1);
    StrokeRounded(dc, inner, std::max(1, radius - 1), kBorder);
}

void FillSolid(HDC dc, const RECT& rect, COLORREF color) {
    HBRUSH brush = ::CreateSolidBrush(color);
    ::FillRect(dc, &rect, brush);
    ::DeleteObject(brush);
}

void DrawTitleBar(HDC dc, HWND window, const std::wstring& title, HFONT title_font,
                  int title_height) {
    RECT client = {};
    ::GetClientRect(window, &client);
    RECT bar = {client.left, client.top, client.right, title_height};
    FillRoundedArgb(dc, bar, 0, RGB(250, 252, 255), 225);
    RECT divider = {client.left, title_height - 1, client.right, title_height};
    FillSolid(dc, divider, kDivider);

    const int icon_size = Scale(window, 22);
    HICON icon = AppIcon();
    if (icon != nullptr) {
        ::DrawIconEx(dc, Scale(window, 18),
                     (title_height - icon_size) / 2, icon, icon_size, icon_size,
                     0, nullptr, DI_NORMAL);
    }

    ::SetBkMode(dc, TRANSPARENT);
    ::SetTextColor(dc, kText);
    HGDIOBJ old_font = ::SelectObject(dc, title_font != nullptr
                                              ? title_font
                                              : ::GetStockObject(DEFAULT_GUI_FONT));
    RECT title_rect = {Scale(window, 52), client.top,
                       client.right - Scale(window, 70), title_height};
    ::DrawTextW(dc, title.c_str(), -1, &title_rect,
                DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS |
                    DT_NOPREFIX);
    ::SelectObject(dc, old_font);

    RECT close = CloseButtonRect(window, title_height);
    if (IsCloseHot(window)) {
        FillRounded(dc, close, Scale(window, 8), RGB(253, 231, 233));
    }
    DrawCloseGlyph(dc, close, IsCloseHot(window) ? kDanger : kTextSecondary);
}

RECT CloseButtonRect(HWND window, int title_height) {
    const int size = Scale(window, 34);
    const int margin = Scale(window, 12);
    RECT client = {};
    ::GetClientRect(window, &client);
    const int top = (title_height - size) / 2;
    return RECT{client.right - margin - size, top, client.right - margin,
                top + size};
}

LRESULT HandleChromeMessage(HWND window, UINT message, WPARAM wparam, LPARAM lparam,
                            int title_height) {
    (void)wparam;
    switch (message) {
        case WM_NCHITTEST: {
            POINT screen = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
            POINT client = screen;
            ::ScreenToClient(window, &client);
            if (PointInRect(CloseButtonRect(window, title_height), client)) {
                return HTCLIENT;
            }
            if (client.y >= 0 && client.y < title_height) {
                return HTCAPTION;
            }
            return HTCLIENT;
        }
        case WM_MOUSEMOVE: {
            POINT point = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
            RECT close = CloseButtonRect(window, title_height);
            SetCloseHot(window, PointInRect(close, point));
            TRACKMOUSEEVENT track = {sizeof(TRACKMOUSEEVENT), TME_LEAVE, window,
                                     0};
            ::TrackMouseEvent(&track);
            return -1;
        }
        case WM_MOUSELEAVE:
            SetCloseHot(window, false);
            return -1;
        case WM_LBUTTONUP: {
            POINT point = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
            if (PointInRect(CloseButtonRect(window, title_height), point)) {
                ::PostMessageW(window, WM_CLOSE, 0, 0);
                return 0;
            }
            return -1;
        }
        default:
            return -1;
    }
}

void DrawNavItem(HDC dc, const RECT& rect, const std::wstring& text, HFONT font,
                 bool selected, bool hover) {
    if (selected) {
        FillRounded(dc, rect, Scale(::GetActiveWindow(), 8), kAccentSoft);
        RECT indicator = {rect.left, rect.top + 4, rect.left + 3,
                          rect.bottom - 4};
        ::FillRect(dc, &indicator, ::CreateSolidBrush(kAccent));
    } else if (hover) {
        FillRounded(dc, rect, Scale(::GetActiveWindow(), 8), RGB(237, 245, 255));
    }
    ::SetBkMode(dc, TRANSPARENT);
    ::SetTextColor(dc, selected ? kAccent : kText);
    HGDIOBJ old_font = ::SelectObject(dc, font != nullptr ? font
                                                         : ::GetStockObject(DEFAULT_GUI_FONT));
    RECT text_rect = rect;
    text_rect.left += Scale(::GetActiveWindow(), 16);
    text_rect.right -= Scale(::GetActiveWindow(), 12);
    ::DrawTextW(dc, text.c_str(), -1, &text_rect,
                DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS |
                    DT_NOPREFIX);
    ::SelectObject(dc, old_font);
}

void DrawButton(HDC dc, const RECT& rect, const std::wstring& text, HFONT font,
                bool primary, bool hover, bool pressed, bool focused,
                bool enabled) {
    const COLORREF fill =
        !enabled ? RGB(245, 247, 250)
                 : primary
                       ? (pressed ? kAccentPressed
                                  : (hover ? kAccentHover : kAccent))
                       : (pressed ? RGB(232, 236, 242)
                                  : (hover ? RGB(246, 249, 253) : kSurface));
    RECT r = InsetBy(rect, 1);
    FillRounded(dc, r, Scale(::GetActiveWindow(), 8), fill);
    if (!primary) {
        StrokeRounded(dc, r, Scale(::GetActiveWindow(), 8),
                      enabled ? kBorder : RGB(212, 218, 227));
    }
    if (focused && enabled) {
        RECT focus = r;
        ::InflateRect(&focus, 1, 1);
        StrokeRounded(dc, focus, Scale(::GetActiveWindow(), 9), kAccent, 2);
    }
    ::SetBkMode(dc, TRANSPARENT);
    ::SetTextColor(dc, !enabled ? kTextDisabled
                                : (primary ? RGB(255, 255, 255) : kText));
    HGDIOBJ old_font = ::SelectObject(dc, font != nullptr ? font
                                                         : ::GetStockObject(DEFAULT_GUI_FONT));
    RECT text_rect = rect;
    ::DrawTextW(dc, text.c_str(), -1, &text_rect,
                DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS |
                    DT_NOPREFIX);
    ::SelectObject(dc, old_font);
}

void DrawToggle(HDC dc, const RECT& rect, const std::wstring& text, HFONT font,
                bool on, bool hover, bool focused) {
    const int dpi = ::GetDpiForWindow(::GetActiveWindow());
    const int track_w = ::MulDiv(38, dpi <= 0 ? 96 : dpi, 96);
    const int track_h = ::MulDiv(20, dpi <= 0 ? 96 : dpi, 96);
    RECT track = {rect.left + Scale(::GetActiveWindow(), 2),
                  rect.top + (rect.bottom - rect.top - track_h) / 2,
                  rect.left + Scale(::GetActiveWindow(), 2) + track_w,
                  rect.top + (rect.bottom - rect.top + track_h) / 2};
    FillRounded(dc, track, track_h / 2,
                on ? kAccent
                   : (hover ? RGB(150, 158, 170) : RGB(164, 172, 184)));
    const int knob = track_h - Scale(::GetActiveWindow(), 4);
    const int knob_x = on ? track.right - knob - Scale(::GetActiveWindow(), 2)
                          : track.left + Scale(::GetActiveWindow(), 2);
    RECT knob_rect = {knob_x, track.top + Scale(::GetActiveWindow(), 2),
                      knob_x + knob, track.top + Scale(::GetActiveWindow(), 2) + knob};
    FillRounded(dc, knob_rect, knob / 2, RGB(255, 255, 255));
    if (focused) {
        RECT focus = track;
        ::InflateRect(&focus, Scale(::GetActiveWindow(), 2),
                      Scale(::GetActiveWindow(), 2));
        StrokeRounded(dc, focus, track_h / 2 + 2, kAccent, 2);
    }
    ::SetBkMode(dc, TRANSPARENT);
    ::SetTextColor(dc, kText);
    HGDIOBJ old_font = ::SelectObject(dc, font != nullptr ? font
                                                         : ::GetStockObject(DEFAULT_GUI_FONT));
    RECT text_rect = {track.right + Scale(::GetActiveWindow(), 12), rect.top,
                      rect.right, rect.bottom};
    ::DrawTextW(dc, text.c_str(), -1, &text_rect,
                DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS |
                    DT_NOPREFIX);
    ::SelectObject(dc, old_font);
}

void DrawRadio(HDC dc, const RECT& rect, const std::wstring& text, HFONT font,
               bool checked, bool hover, bool focused) {
    const int dpi = ::GetDpiForWindow(::GetActiveWindow());
    const int size = ::MulDiv(18, dpi <= 0 ? 96 : dpi, 96);
    RECT circle = {rect.left + Scale(::GetActiveWindow(), 2),
                   rect.top + (rect.bottom - rect.top - size) / 2,
                   rect.left + Scale(::GetActiveWindow(), 2) + size,
                   rect.top + (rect.bottom - rect.top + size) / 2};
    FillRounded(dc, circle, size / 2, checked ? kAccent : RGB(255, 255, 255));
    StrokeRounded(dc, circle, size / 2, checked ? kAccent : RGB(152, 160, 172));
    if (checked) {
        RECT dot = circle;
        ::InflateRect(&dot, -Scale(::GetActiveWindow(), 5),
                      -Scale(::GetActiveWindow(), 5));
        FillRounded(dc, dot, (dot.bottom - dot.top) / 2, RGB(255, 255, 255));
    }
    if (focused || hover) {
        RECT ring = circle;
        ::InflateRect(&ring, Scale(::GetActiveWindow(), 1),
                      Scale(::GetActiveWindow(), 1));
        StrokeRounded(dc, ring, size / 2 + 1, kAccent);
    }
    ::SetBkMode(dc, TRANSPARENT);
    ::SetTextColor(dc, kText);
    HGDIOBJ old_font = ::SelectObject(dc, font != nullptr ? font
                                                         : ::GetStockObject(DEFAULT_GUI_FONT));
    RECT text_rect = {circle.right + Scale(::GetActiveWindow(), 10), rect.top,
                      rect.right, rect.bottom};
    ::DrawTextW(dc, text.c_str(), -1, &text_rect,
                DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS |
                    DT_NOPREFIX);
    ::SelectObject(dc, old_font);
}

void DrawSeparator(HDC dc, const RECT& rect) {
    ::FillRect(dc, &rect, ::CreateSolidBrush(kDivider));
}

HWND CreateToggle(HWND parent, HINSTANCE instance, int id, const std::wstring& text,
                  HFONT font, int width, int height, int dpi) {
    HWND control = ::CreateWindowExW(
        0, WC_BUTTONW, text.c_str(),
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW, 0, 0,
        ::MulDiv(width, dpi, USER_DEFAULT_SCREEN_DPI),
        ::MulDiv(height, dpi, USER_DEFAULT_SCREEN_DPI), parent,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), instance, nullptr);
    if (control == nullptr) {
        return nullptr;
    }
    if (font != nullptr) {
        ::SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    }
    auto* state = new ToggleState();
    state->font = font;
    ::SetWindowLongPtrW(control, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
    ::SetWindowSubclass(control, &ToggleProc, 1, 0);
    return control;
}

HWND CreateButton(HWND parent, HINSTANCE instance, int id, const std::wstring& text,
                  HFONT font, int width, int height, int dpi, bool primary) {
    return CreateCustomButton(parent, instance, id, text, font, width, height,
                              dpi, primary, false);
}

HWND CreateRadio(HWND parent, HINSTANCE instance, int id, const std::wstring& text,
                 HFONT font, int width, int height, int dpi) {
    return CreateCustomButton(parent, instance, id, text, font, width, height,
                              dpi, false, true);
}

}  // namespace fluent
}  // namespace alt
