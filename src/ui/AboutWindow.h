#pragma once

#include <windows.h>

#include <string>

#include "app/Settings.h"
#include "ui/Window.h"

namespace alt {

// About window (original Form\frmAbout.pas, docs/spec/04-aux-features.md §6.1).
//
// Native controls like ConfigWindow: popup/topmost/tool-window, created once by
// App and reused.  Shows the rewrite version, the original project URL, the
// ShortCutRunCount statistic, the ini/data locations and the free-form
// <exe目录>\About.<Lang> text (read-only multiline EDIT with scrolling; the
// built-in text is used when the file is missing).  Esc / the close button /
// WM_CLOSE only hide the window, so it can be reopened without rebuilding.
class AboutWindow : public Window {
public:
    explicit AboutWindow(Settings& settings);
    ~AboutWindow() override;

    bool CreateAboutWindow(HINSTANCE instance);

    // Re-reads About.<Lang> and the dynamic values, then shows the window.
    void Open();
    // Refreshes texts without showing (used by App::ApplySettings after a
    // language / shortcut-count change).
    void RefreshTexts();

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
    void PlaceOnCursorMonitor();
    void DrawHeader(HDC dc) const;
    std::wstring AboutFilePath() const;
    void LoadAboutText();

    static LRESULT CALLBACK ChildSubclassProc(HWND hwnd, UINT message,
                                              WPARAM wparam, LPARAM lparam,
                                              UINT_PTR /*subclass_id*/,
                                              DWORD_PTR ref_data);

    Settings& settings_;

    int dpi_ = USER_DEFAULT_SCREEN_DPI;
    HFONT font_ = nullptr;
    HFONT title_font_ = nullptr;
    bool font_is_stock_ = false;
    HBRUSH background_brush_ = nullptr;

    HWND title_text_ = nullptr;
    HWND source_text_ = nullptr;
    HWND run_count_text_ = nullptr;
    HWND ini_path_text_ = nullptr;
    HWND data_dir_text_ = nullptr;
    HWND content_label_ = nullptr;
    HWND content_edit_ = nullptr;
    HWND close_button_ = nullptr;
};

}  // namespace alt
