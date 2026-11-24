#pragma once

#include <windows.h>

#include <functional>

#include "app/Settings.h"
#include "ui/Window.h"

namespace alt {

// First-run welcome window (original frmLang + the AutoRun / AddToSendTo
// questions in frmALTRun.pas, plus a short usage guide the original never had).
//
// Shown once, when Settings::Load() found no ALTRun.ini.  It offers the three
// built-in UI languages and the two shell integrations; accepting writes the
// ini (so the window does not appear again), applies Settings and returns to
// the tray.  Native controls, created once and reused like the other dialogs.
class WelcomeWindow : public Window {
public:
    explicit WelcomeWindow(Settings& settings);
    ~WelcomeWindow() override;

    bool CreateWelcomeWindow(HINSTANCE instance);

    // Refreshes texts from the current locale/settings and shows the window.
    void Open();

    // Called after the user confirms; App uses it to save + apply settings.
    void SetAcceptHandler(std::function<void()> handler) {
        accept_handler_ = std::move(handler);
    }

protected:
    LRESULT HandleMessage(UINT message, WPARAM wparam, LPARAM lparam) override;
    void OnCreate() override;

private:
    int S(int value) const;
    void CreateFonts();
    void DestroyResources();
    void CreateControls();
    HWND MakeControl(const wchar_t* class_name, const wchar_t* text, DWORD style,
                     int x, int y, int width, int height, int id);
    void ApplyFont(HWND control) const;
    void RefreshTexts();
    void PlaceOnCursorMonitor();
    void DrawHeader(HDC dc) const;
    void Accept();

    Settings& settings_;

    int dpi_ = USER_DEFAULT_SCREEN_DPI;
    HFONT font_ = nullptr;
    HFONT title_font_ = nullptr;
    bool font_is_stock_ = false;
    HBRUSH background_brush_ = nullptr;

    HWND intro_text_ = nullptr;
    HWND guide_text_ = nullptr;
    HWND language_label_ = nullptr;
    HWND language_combo_ = nullptr;
    HWND auto_run_check_ = nullptr;
    HWND send_to_check_ = nullptr;
    HWND note_text_ = nullptr;
    HWND start_button_ = nullptr;

    std::function<void()> accept_handler_;
};

}  // namespace alt
