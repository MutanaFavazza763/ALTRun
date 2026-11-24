#pragma once

#include <windows.h>

#include <functional>
#include <string>

#include "core/Shortcut.h"
#include "ui/Window.h"

namespace alt {

// Single-item editor (original Form\frmShortCut.pas). Native controls, like
// ConfigWindow: an entry form is exactly the place where native EDIT/BUTTON
// give IME, theming and keyboard behaviour for free.
//
// The dialog is modal: Open() runs a nested message loop until the user
// confirms or cancels, and temporarily disables the owner window. The confirm
// handler runs only for valid input (see Confirm()); cancel/Esc/X close the
// dialog without invoking it.
class ShortcutEditWindow : public Window {
public:
    using ConfirmHandler = std::function<void(const Shortcut& item, bool is_blank)>;

    ShortcutEditWindow();
    ~ShortcutEditWindow() override;

    bool CreateEditWindow(HINSTANCE instance);

    // |item| == nullptr means "new item". |on_confirm| is called after the
    // user pressed 确定 and validation passed; it must not keep the pointer.
    void Open(const Shortcut* item, ConfirmHandler on_confirm);

protected:
    LRESULT HandleMessage(UINT message, WPARAM wparam, LPARAM lparam) override;
    void OnCreate() override;
    void OnPaint() override;

private:
    int S(int value) const;

    void CreateFonts();
    void DestroyResources();
    void CreateControls();
    HWND MakeControl(const wchar_t* class_name, const wchar_t* text, DWORD style,
                     int x, int y, int width, int height, int id);
    void ApplyFont(HWND control) const;

    void LoadFromShortcut(const Shortcut& item, bool is_new);
    Shortcut CurrentShortcut() const;
    std::string ReadTrimmed(HWND control) const;

    void Confirm();
    void Cancel();
    void Finish(const Shortcut& item, bool is_blank);
    void TestCommandLine();
    void PickFile();
    void PickFolder();
    void ApplyPickedPath(const std::wstring& path);

    void MoveFocus(bool backwards);
    void FocusFirstControl();
    void PlaceOnOwner();
    void DrawHeader(HDC dc) const;
    void PostButtonCommand(int id) const;

    static LRESULT CALLBACK ChildSubclassProc(HWND hwnd, UINT message,
                                              WPARAM wparam, LPARAM lparam,
                                              UINT_PTR /*subclass_id*/,
                                              DWORD_PTR ref_data);

    int dpi_ = USER_DEFAULT_SCREEN_DPI;
    HFONT font_ = nullptr;
    HFONT title_font_ = nullptr;
    bool font_is_stock_ = false;
    HBRUSH background_brush_ = nullptr;

    HWND keyword_edit_ = nullptr;
    HWND name_edit_ = nullptr;
    HWND command_line_edit_ = nullptr;
    HWND radio_none_ = nullptr;
    HWND radio_no_encoding_ = nullptr;
    HWND radio_url_query_ = nullptr;
    HWND radio_utf8_query_ = nullptr;
    HWND pick_file_button_ = nullptr;
    HWND pick_folder_button_ = nullptr;
    HWND test_button_ = nullptr;
    HWND ok_button_ = nullptr;
    HWND cancel_button_ = nullptr;
    HWND hint_label_ = nullptr;

    bool modal_ = false;
    bool editing_new_ = true;
    Shortcut original_;
    HWND owner_ = nullptr;
    ConfirmHandler on_confirm_;
};

}  // namespace alt
