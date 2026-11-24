#pragma once

#include <windows.h>

#include <array>
#include <functional>
#include <string>
#include <vector>

#include "app/Settings.h"
#include "ui/Window.h"

namespace alt {

// Settings dialog (original Form\frmConfig.pas, reduced to the tabs the C++
// rewrite actually persists).
//
// Unlike MainWindow/ParamWindow this window deliberately uses **native**
// controls instead of Direct2D: SysTabControl32 pages built from BUTTON /
// EDIT / COMBOBOX / STATIC. The original VCL dialog was made of the same
// controls, and native controls give keyboard handling, IME, theming and
// accessibility for free.
//
// Value flow (mirrors the original):
//   * Open()       -> Settings -> controls (nothing is written back here)
//   * 确定          -> controls -> Settings -> Settings::Save() -> apply handler
//   * 取消 / Esc / WM_CLOSE -> hide only, settings are left untouched
// The window is created once and reused; "closing" merely hides it, exactly
// like ParamWindow.
class ConfigWindow : public Window {
public:
    explicit ConfigWindow(Settings& settings);
    ~ConfigWindow() override;

    bool CreateConfigWindow(HINSTANCE instance);

    // Loads the current Settings into the controls and shows the window.
    void Open();

    // Called after 确定 has written Settings and ALTRun.ini. App uses it to
    // re-register hotkeys, refresh the launcher and re-apply system
    // integration (AutoRun / AddToSendTo / tray icon / ...).
    void SetApplyHandler(std::function<void()> handler);

protected:
    LRESULT HandleMessage(UINT message, WPARAM wparam, LPARAM lparam) override;
    void OnCreate() override;

private:
    static constexpr int kPageCount = 4;
    static constexpr int kPageGeneral = 0;
    static constexpr int kPageHotkey = 1;
    static constexpr int kPageAppearance = 2;
    static constexpr int kPageAbout = 3;

    // One control bound to one Settings field; keeps load/save free of a
    // per-control if-ladder.
    struct BoolBinding {
        HWND hwnd = nullptr;
        bool Settings::*field = nullptr;
    };
    struct IntBinding {
        HWND hwnd = nullptr;
        int Settings::*field = nullptr;
        int min_value = 0;
        int max_value = 0;
    };
    struct TextBinding {
        HWND hwnd = nullptr;
        std::string Settings::*field = nullptr;
    };

    // --- layout / control creation ---------------------------------------
    int S(int value) const;
    void CreateFonts();
    void DestroyResources();
    void CreateControls();
    void CreateGeneralPage();
    void CreateHotkeyPage();
    void CreateAppearancePage();
    void CreateAboutPage();
    HWND MakeControl(int page, const wchar_t* class_name,
                     const std::wstring& text, DWORD style, int x, int y,
                     int width, int height, int id);
    HWND AddLabel(int page, const std::wstring& text, int x, int y, int width,
                  int height = 24, DWORD extra_style = SS_LEFT);
    HWND AddCheckBox(int page, const std::wstring& text, int x, int y, int width);
    HWND AddIntEdit(int page, const std::wstring& label, int Settings::*field,
                    int min_value, int max_value, int x, int y, int label_width,
                    int edit_width);
    HWND AddTextEdit(int page, const std::wstring& label,
                     std::string Settings::*field, int x, int y, int label_width,
                     int edit_width);
    HWND AddHotkeyEdit(int page, const std::wstring& label,
                       std::string Settings::*field, int x, int y, int label_width,
                       int edit_width);
    void ApplyFont(HWND control) const;

    // --- value flow / behaviour -------------------------------------------
    void LoadFromSettings();
    void SaveFromControls();
    void UpdateAboutTexts();
    void Confirm();
    void Cancel();
    void SelectPage(int index);
    void MoveFocus(bool backwards);
    void FocusFirstControl();
    void PlaceOnCursorMonitor();
    void DrawHeader(HDC dc) const;
    // Posts BM_CLICK-like WM_COMMAND so button handling stays re-entrancy safe
    // when triggered from a child control subclass.
    void PostButtonCommand(int id) const;

    static LRESULT CALLBACK ChildSubclassProc(HWND hwnd, UINT message,
                                              WPARAM wparam, LPARAM lparam,
                                              UINT_PTR /*subclass_id*/,
                                              DWORD_PTR ref_data);
    static LRESULT CALLBACK HotkeySubclassProc(HWND hwnd, UINT message,
                                               WPARAM wparam, LPARAM lparam,
                                               UINT_PTR /*subclass_id*/,
                                               DWORD_PTR ref_data);

    Settings& settings_;
    std::function<void()> apply_handler_;

    int dpi_ = 96;
    HFONT font_ = nullptr;
    HFONT title_font_ = nullptr;
    bool font_is_stock_ = false;
    HBRUSH background_brush_ = nullptr;

    HWND tab_ = nullptr;
    HWND ok_button_ = nullptr;
    HWND cancel_button_ = nullptr;
    HWND version_text_ = nullptr;
    HWND ini_path_text_ = nullptr;
    HWND run_count_text_ = nullptr;

    std::array<std::vector<HWND>, kPageCount> page_controls_ = {};
    std::vector<BoolBinding> bools_;
    std::vector<IntBinding> ints_;
    // Plain text fields AND the three read-only hotkey boxes (their captured
    // text is already in the legacy "Alt + R" notation, so saving is just a
    // UTF-8 round-trip).
    std::vector<TextBinding> texts_;
    int current_page_ = kPageGeneral;
};

}  // namespace alt
