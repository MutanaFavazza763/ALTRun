#include "ui/FluentMenu.h"

#include <commctrl.h>
#include <windowsx.h>

#include <algorithm>
#include <utility>

#include "ui/FluentTheme.h"

namespace alt {
namespace fluent {
namespace {

constexpr wchar_t kMenuClass[] = L"ALTRun.FluentMenu.v1";

int ScaleForDpi(int value, int dpi) {
    return ::MulDiv(value, dpi <= 0 ? 96 : dpi, 96);
}

std::wstring StripAccelerator(const std::wstring& text) {
    // Localization keeps the legacy "(&O)" mnemonics.  A custom drawn menu does
    // not need them and would otherwise show the ampersand literally.
    std::wstring result = text;
    const size_t marker = result.find(L"(&");
    if (marker != std::wstring::npos) {
        result.erase(marker);
    }
    while (!result.empty() &&
           (result.back() == L' ' || result.back() == L'\t')) {
        result.pop_back();
    }
    return result;
}

class PopupMenuWindow {
public:
    PopupMenuWindow(HWND owner, HINSTANCE instance, POINT point,
                    std::wstring title, std::wstring subtitle, HICON icon,
                    std::vector<MenuItem> items)
        : owner_(owner),
          instance_(instance),
          point_(point),
          title_(std::move(title)),
          subtitle_(std::move(subtitle)),
          icon_(icon),
          items_(std::move(items)) {
        for (MenuItem& item : items_) {
            item.text = StripAccelerator(item.text);
        }
    }

    ~PopupMenuWindow() { DestroyResources(); }

    int Show() {
        RegisterClassOnce();
        dpi_ = static_cast<int>(::GetDpiForSystem());
        if (dpi_ <= 0) {
            dpi_ = USER_DEFAULT_SCREEN_DPI;
        }
        const int width = ScaleForDpi(260, dpi_);
        const int height = ContentHeight();
        CalculatePosition(width, height);

        hwnd_ = ::CreateWindowExW(
            WS_EX_TOOLWINDOW | WS_EX_TOPMOST, kMenuClass, L"", WS_POPUP,
            position_.x, position_.y, width, height, owner_, nullptr, instance_,
            this);
        if (hwnd_ == nullptr) {
            return 0;
        }

        ::ShowWindow(hwnd_, SW_SHOW);
        ::SetForegroundWindow(hwnd_);
        ::SetFocus(hwnd_);
        ::SetCapture(hwnd_);

        MSG message = {};
        while (!done_) {
            const int result = ::GetMessageW(&message, nullptr, 0, 0);
            if (result == 0) {
                ::PostQuitMessage(static_cast<int>(message.wParam));
                break;
            }
            if (result == -1) {
                break;
            }
            ::TranslateMessage(&message);
            ::DispatchMessageW(&message);
        }
        if (::GetCapture() == hwnd_) {
            ::ReleaseCapture();
        }
        if (hwnd_ != nullptr) {
            ::DestroyWindow(hwnd_);
            hwnd_ = nullptr;
        }
        return result_;
    }

private:
    static void RegisterClassOnce() {
        static bool registered = false;
        if (registered) {
            return;
        }
        WNDCLASSEXW window_class = {};
        window_class.cbSize = sizeof(window_class);
        window_class.style = CS_DROPSHADOW;
        window_class.lpfnWndProc = &PopupMenuWindow::WndProc;
        window_class.hInstance = ::GetModuleHandleW(nullptr);
        window_class.hCursor = ::LoadCursorW(nullptr, IDC_ARROW);
        window_class.lpszClassName = kMenuClass;
        ::RegisterClassExW(&window_class);
        registered = true;
    }

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wparam,
                                    LPARAM lparam) {
        PopupMenuWindow* self = reinterpret_cast<PopupMenuWindow*>(
            ::GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (message == WM_NCCREATE) {
            const auto* create = reinterpret_cast<CREATESTRUCTW*>(lparam);
            self = static_cast<PopupMenuWindow*>(create->lpCreateParams);
            ::SetWindowLongPtrW(hwnd, GWLP_USERDATA,
                                reinterpret_cast<LONG_PTR>(self));
            if (self != nullptr) {
                self->hwnd_ = hwnd;
            }
        }
        if (self == nullptr) {
            return ::DefWindowProcW(hwnd, message, wparam, lparam);
        }
        return self->Handle(message, wparam, lparam);
    }

    LRESULT Handle(UINT message, WPARAM wparam, LPARAM lparam) {
        switch (message) {
            case WM_CREATE:
                CreateResources();
                SetWindowRegion();
                return 0;
            case WM_ERASEBKGND:
                return 1;
            case WM_PAINT:
                Paint();
                return 0;
            case WM_MOUSEMOVE: {
                const int hover = HitTest(GET_X_LPARAM(lparam),
                                          GET_Y_LPARAM(lparam));
                if (hover != hover_) {
                    hover_ = hover;
                    keyboard_ = false;
                    ::InvalidateRect(hwnd_, nullptr, FALSE);
                }
                TRACKMOUSEEVENT track = {sizeof(TRACKMOUSEEVENT), TME_LEAVE,
                                         hwnd_, 0};
                ::TrackMouseEvent(&track);
                return 0;
            }
            case WM_MOUSELEAVE:
                if (hover_ != -1) {
                    hover_ = -1;
                    ::InvalidateRect(hwnd_, nullptr, FALSE);
                }
                return 0;
            case WM_LBUTTONDOWN:
                ::SetCapture(hwnd_);
                return 0;
            case WM_LBUTTONUP: {
                if (::GetCapture() == hwnd_) {
                    ::ReleaseCapture();
                }
                RECT client = {};
                ::GetClientRect(hwnd_, &client);
                POINT point = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
                if (!::PtInRect(&client, point)) {
                    Dismiss(0);
                    return 0;
                }
                const int index = HitTest(point.x, point.y);
                Activate(index);
                return 0;
            }
            case WM_KEYDOWN:
                HandleKey(static_cast<UINT>(wparam));
                return 0;
            case WM_CHAR:
                return 0;
            case WM_ACTIVATE:
                if (LOWORD(wparam) == WA_INACTIVE && !dismissing_) {
                    Dismiss(0);
                }
                return 0;
            case WM_CLOSE:
                Dismiss(0);
                return 0;
            case WM_DESTROY:
                DestroyResources();
                return 0;
            case WM_NCDESTROY:
                ::SetWindowLongPtrW(hwnd_, GWLP_USERDATA, 0);
                break;
            default:
                break;
        }
        return ::DefWindowProcW(hwnd_, message, wparam, lparam);
    }

    int ContentHeight() const {
        const int header = ScaleForDpi(58, dpi_);
        const int padding = ScaleForDpi(6, dpi_);
        int content = 0;
        for (const MenuItem& item : items_) {
            content += item.separator ? ScaleForDpi(9, dpi_)
                                     : ScaleForDpi(38, dpi_);
        }
        return header + padding * 2 + content;
    }

    void CalculatePosition(int width, int height) {
        HMONITOR monitor =
            ::MonitorFromPoint(point_, MONITOR_DEFAULTTONEAREST);
        MONITORINFO info = {};
        info.cbSize = sizeof(info);
        ::GetMonitorInfoW(monitor, &info);
        int x = point_.x;
        int y = point_.y;
        if (x + width > info.rcWork.right) {
            x = info.rcWork.right - width;
        }
        if (y + height > info.rcWork.bottom) {
            y = info.rcWork.bottom - height;
        }
        x = std::max(x, static_cast<int>(info.rcWork.left));
        y = std::max(y, static_cast<int>(info.rcWork.top));
        position_ = {x, y};
    }

    void CreateResources() {
        dpi_ = static_cast<int>(::GetDpiForWindow(hwnd_));
        if (dpi_ <= 0) {
            dpi_ = USER_DEFAULT_SCREEN_DPI;
        }
        auto make_font = [this](int points, int weight, const wchar_t* family) {
            return ::CreateFontW(
                -::MulDiv(points, dpi_, 72), 0, 0, 0, weight, FALSE, FALSE,
                FALSE, DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, family);
        };
        title_font_ = make_font(11, FW_SEMIBOLD, L"Segoe UI Variable Text");
        font_ = make_font(10, FW_NORMAL, L"Segoe UI Variable Text");
        small_font_ = make_font(9, FW_NORMAL, L"Segoe UI Variable Text");
        glyph_font_ = make_font(11, FW_NORMAL, L"Segoe UI Symbol");

        const int width = ScaleForDpi(260, dpi_);
        const int height = ContentHeight();
        ::SetWindowPos(hwnd_, HWND_TOPMOST, position_.x, position_.y, width,
                       height, SWP_NOACTIVATE);
        SetWindowRegion();
    }

    void DestroyResources() {
        for (HFONT* font : {&title_font_, &font_, &small_font_, &glyph_font_}) {
            if (*font != nullptr) {
                ::DeleteObject(*font);
                *font = nullptr;
            }
        }
    }

    void SetWindowRegion() {
        RECT client = {};
        ::GetClientRect(hwnd_, &client);
        const int radius = ScaleForDpi(10, dpi_);
        HRGN region = ::CreateRoundRectRgn(0, 0, client.right + 1,
                                           client.bottom + 1, radius, radius);
        if (region != nullptr) {
            ::SetWindowRgn(hwnd_, region, TRUE);
        }
    }

    int HitTest(int x, int y) const {
        if (x < 0 || y < header_height()) {
            return -1;
        }
        const int padding = ScaleForDpi(6, dpi_);
        int top = header_height() + padding;
        for (int index = 0; index < static_cast<int>(items_.size()); ++index) {
            const int height = items_[index].separator
                                   ? ScaleForDpi(9, dpi_)
                                   : ScaleForDpi(38, dpi_);
            if (y >= top && y < top + height) {
                return items_[index].separator ? -1 : index;
            }
            top += height;
        }
        return -1;
    }

    int header_height() const { return ScaleForDpi(58, dpi_); }

    void Paint() {
        PAINTSTRUCT paint = {};
        HDC dc = ::BeginPaint(hwnd_, &paint);
        RECT client = {};
        ::GetClientRect(hwnd_, &client);

        HDC memory = ::CreateCompatibleDC(dc);
        HBITMAP bitmap = ::CreateCompatibleBitmap(dc, client.right, client.bottom);
        HGDIOBJ old_bitmap = ::SelectObject(memory, bitmap);

        // White surface with a very light blue tint keeps the popup in the same
        // family as the settings cards without looking like a native HMENU.
        HBRUSH background = ::CreateSolidBrush(kSurface);
        ::FillRect(memory, &client, background);
        ::DeleteObject(background);
        StrokeRounded(memory, RECT{0, 0, client.right, client.bottom},
                      ScaleForDpi(10, dpi_), kBorder);

        DrawHeader(memory, client);
        DrawItems(memory, client);

        ::BitBlt(dc, 0, 0, client.right, client.bottom, memory, 0, 0, SRCCOPY);
        ::SelectObject(memory, old_bitmap);
        ::DeleteObject(bitmap);
        ::DeleteDC(memory);
        ::EndPaint(hwnd_, &paint);
    }

    void DrawHeader(HDC dc, const RECT& client) {
        const int header = header_height();
        const int icon_size = ScaleForDpi(24, dpi_);
        const int icon_x = ScaleForDpi(16, dpi_);
        const int icon_y = (header - icon_size) / 2;
        if (icon_ != nullptr) {
            ::DrawIconEx(dc, icon_x, icon_y, icon_, icon_size, icon_size, 0,
                         nullptr, DI_NORMAL);
        } else {
            ::SetBkMode(dc, TRANSPARENT);
            ::SetTextColor(dc, kAccent);
            HGDIOBJ old = ::SelectObject(dc, glyph_font_);
            RECT star = {icon_x, icon_y, icon_x + icon_size,
                         icon_y + icon_size};
            ::DrawTextW(dc, L"\u2726", -1, &star,
                        DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
            ::SelectObject(dc, old);
        }

        const int text_x = ScaleForDpi(50, dpi_);
        ::SetBkMode(dc, TRANSPARENT);
        ::SetTextColor(dc, kText);
        HGDIOBJ old = ::SelectObject(dc, title_font_);
        RECT title = {text_x, ScaleForDpi(9, dpi_), client.right - ScaleForDpi(16, dpi_),
                      ScaleForDpi(31, dpi_)};
        ::DrawTextW(dc, title_.c_str(), -1, &title,
                    DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS |
                        DT_NOPREFIX);

        ::SelectObject(dc, small_font_);
        ::SetTextColor(dc, kTextSecondary);
        RECT subtitle = {text_x, ScaleForDpi(29, dpi_),
                         client.right - ScaleForDpi(16, dpi_),
                         ScaleForDpi(51, dpi_)};
        ::DrawTextW(dc, subtitle_.c_str(), -1, &subtitle,
                    DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS |
                        DT_NOPREFIX);
        ::SelectObject(dc, old);

        DrawSeparator(dc, RECT{ScaleForDpi(14, dpi_), header - 1,
                              client.right - ScaleForDpi(14, dpi_), header});
    }

    void DrawItems(HDC dc, const RECT& client) {
        const int padding = ScaleForDpi(6, dpi_);
        int top = header_height() + padding;
        for (int index = 0; index < static_cast<int>(items_.size()); ++index) {
            const MenuItem& item = items_[index];
            const int height =
                item.separator ? ScaleForDpi(9, dpi_) : ScaleForDpi(38, dpi_);
            const RECT row = {ScaleForDpi(6, dpi_), top,
                             client.right - ScaleForDpi(6, dpi_),
                             top + height};
            if (item.separator) {
                DrawSeparator(dc, RECT{ScaleForDpi(16, dpi_),
                                      top + height / 2,
                                      client.right - ScaleForDpi(16, dpi_),
                                      top + height / 2 + 1});
                top += height;
                continue;
            }

            const bool hot = index == hover_;
            if (hot) {
                FillRounded(dc, row, ScaleForDpi(7, dpi_),
                            item.danger ? RGB(253, 231, 233) : kAccentSoft);
                if (keyboard_) {
                    StrokeRounded(dc, row, ScaleForDpi(7, dpi_), kAccent);
                }
            }

            ::SetBkMode(dc, TRANSPARENT);
            ::SetTextColor(dc, !item.enabled ? kTextDisabled
                                             : (item.danger ? kDanger : kText));
            HGDIOBJ old = ::SelectObject(dc, font_);
            RECT label = {ScaleForDpi(50, dpi_), top,
                          client.right - ScaleForDpi(16, dpi_),
                          top + height};
            ::DrawTextW(dc, item.text.c_str(), -1, &label,
                        DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS |
                            DT_NOPREFIX);
            ::SelectObject(dc, old);

            if (!item.glyph.empty()) {
                ::SetTextColor(dc, !item.enabled ? kTextDisabled
                                                 : (item.danger ? kDanger
                                                                : kAccent));
                old = ::SelectObject(dc, glyph_font_);
                RECT glyph = {ScaleForDpi(18, dpi_), top,
                              ScaleForDpi(42, dpi_), top + height};
                ::DrawTextW(dc, item.glyph.c_str(), -1, &glyph,
                            DT_CENTER | DT_SINGLELINE | DT_VCENTER |
                                DT_NOPREFIX);
                ::SelectObject(dc, old);
            }
            top += height;
        }
    }

    void HandleKey(UINT key) {
        if (key == VK_ESCAPE) {
            Dismiss(0);
            return;
        }
        if (key == VK_RETURN) {
            Activate(hover_);
            return;
        }
        if (key == VK_UP || key == VK_DOWN) {
            int index = hover_;
            const int direction = key == VK_DOWN ? 1 : -1;
            for (int step = 0; step < static_cast<int>(items_.size()); ++step) {
                index += direction;
                if (index < 0) {
                    index = static_cast<int>(items_.size()) - 1;
                }
                if (index >= static_cast<int>(items_.size())) {
                    index = 0;
                }
                if (!items_[index].separator && items_[index].enabled) {
                    hover_ = index;
                    keyboard_ = true;
                    ::InvalidateRect(hwnd_, nullptr, FALSE);
                    return;
                }
            }
        }
    }

    void Activate(int index) {
        if (index < 0 || index >= static_cast<int>(items_.size())) {
            Dismiss(0);
            return;
        }
        const MenuItem& item = items_[index];
        if (item.separator || !item.enabled) {
            return;
        }
        Dismiss(item.id);
    }

    void Dismiss(int result) {
        if (dismissing_) {
            return;
        }
        dismissing_ = true;
        result_ = result;
        done_ = true;
    }

    HWND owner_ = nullptr;
    HINSTANCE instance_ = nullptr;
    POINT point_ = {};
    POINT position_ = {};
    std::wstring title_;
    std::wstring subtitle_;
    HICON icon_ = nullptr;
    std::vector<MenuItem> items_;
    HWND hwnd_ = nullptr;
    int dpi_ = 96;
    int hover_ = -1;
    int result_ = 0;
    bool keyboard_ = false;
    bool done_ = false;
    bool dismissing_ = false;
    HFONT title_font_ = nullptr;
    HFONT font_ = nullptr;
    HFONT small_font_ = nullptr;
    HFONT glyph_font_ = nullptr;
};

}  // namespace

int ShowPopupMenu(HWND owner, HINSTANCE instance, POINT screen_point,
                  const std::wstring& title, const std::wstring& subtitle,
                  HICON icon, const std::vector<MenuItem>& items) {
    PopupMenuWindow window(owner, instance, screen_point, title, subtitle, icon,
                           items);
    return window.Show();
}

}  // namespace fluent
}  // namespace alt
