#pragma once

#include <windows.h>

#include <functional>
#include <string>

namespace alt {

// Thin wrapper around a Win32 window that routes messages to virtual handlers.
//
// Every ALTRun window is a borderless popup that paints itself with Direct2D
// (the original VCL application used a skinned TForm with a PNG background;
// the rewrite keeps the look but drops the skin engine).
class Window {
public:
    Window() = default;
    virtual ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    bool Create(HINSTANCE instance, const std::wstring& class_name,
                const std::wstring& title, DWORD style, DWORD ex_style);
    void Destroy();

    HWND hwnd() const { return hwnd_; }
    bool visible() const { return ::IsWindowVisible(hwnd_) != FALSE; }

    void Show();
    void Hide();

    // Called for messages the subclass did not handle. Used by App to receive
    // tray icon and hotkey notifications on the main window.
    void SetExternalHandler(
        std::function<LRESULT(HWND, UINT, WPARAM, LPARAM)> handler);

protected:
    virtual LRESULT HandleMessage(UINT message, WPARAM wparam, LPARAM lparam);
    virtual void OnCreate() {}
    virtual void OnPaint() {}

    HWND hwnd_ = nullptr;
    HINSTANCE instance_ = nullptr;
    std::wstring class_name_;
    std::function<LRESULT(HWND, UINT, WPARAM, LPARAM)> external_handler_;

private:
    static LRESULT CALLBACK StaticWndProc(HWND hwnd, UINT message, WPARAM wparam,
                                          LPARAM lparam);
};

}  // namespace alt
