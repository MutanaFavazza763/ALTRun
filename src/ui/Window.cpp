#include "ui/Window.h"

#include "core/Logger.h"
#include "core/Text.h"

namespace alt {

Window::~Window() { Destroy(); }

bool Window::Create(HINSTANCE instance, const std::wstring& class_name,
                    const std::wstring& title, DWORD style, DWORD ex_style) {
    instance_ = instance;
    class_name_ = class_name;

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wc.lpfnWndProc = &Window::StaticWndProc;
    wc.hInstance = instance;
    wc.hCursor = ::LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    wc.lpszClassName = class_name_.c_str();

    if (::RegisterClassExW(&wc) == 0) {
        const DWORD error = ::GetLastError();
        if (error != ERROR_CLASS_ALREADY_EXISTS) {
            Logger::Instance().Error("RegisterClassEx failed: " + std::to_string(error));
            return false;
        }
    }

    hwnd_ = ::CreateWindowExW(ex_style, class_name_.c_str(), title.c_str(), style,
                              CW_USEDEFAULT, CW_USEDEFAULT, 400, 120, nullptr,
                              nullptr, instance, this);
    if (hwnd_ == nullptr) {
        Logger::Instance().Error("CreateWindowEx failed");
        return false;
    }

    ::SetWindowLongPtrW(hwnd_, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    return true;
}

void Window::Destroy() {
    if (hwnd_ != nullptr) {
        const HWND hwnd = hwnd_;
        hwnd_ = nullptr;
        ::DestroyWindow(hwnd);
    }
}

void Window::Show() {
    if (hwnd_ != nullptr) {
        ::ShowWindow(hwnd_, SW_SHOWNORMAL);
        ::SetForegroundWindow(hwnd_);
    }
}

void Window::Hide() {
    if (hwnd_ != nullptr) {
        ::ShowWindow(hwnd_, SW_HIDE);
    }
}

void Window::SetExternalHandler(
    std::function<LRESULT(HWND, UINT, WPARAM, LPARAM)> handler) {
    external_handler_ = std::move(handler);
}

LRESULT Window::HandleMessage(UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
        case WM_CREATE:
            OnCreate();
            return 0;
        case WM_PAINT:
            OnPaint();
            return 0;
        case WM_ERASEBKGND:
            return 1;  // fully painted by Direct2D
        default:
            break;
    }
    if (external_handler_) {
        return external_handler_(hwnd_, message, wparam, lparam);
    }
    return ::DefWindowProcW(hwnd_, message, wparam, lparam);
}

LRESULT CALLBACK Window::StaticWndProc(HWND hwnd, UINT message, WPARAM wparam,
                                       LPARAM lparam) {
    if (message == WM_NCCREATE) {
        auto* create = reinterpret_cast<CREATESTRUCTW*>(lparam);
        auto* self = static_cast<Window*>(create->lpCreateParams);
        if (self != nullptr) {
            self->hwnd_ = hwnd;
            ::SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        }
    }

    auto* self = reinterpret_cast<Window*>(::GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (self != nullptr) {
        return self->HandleMessage(message, wparam, lparam);
    }
    return ::DefWindowProcW(hwnd, message, wparam, lparam);
}

}  // namespace alt
