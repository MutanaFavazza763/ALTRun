#pragma once

#include <windows.h>

#include <functional>
#include <memory>
#include <string>

#include "app/Settings.h"
#include "core/ShortcutStore.h"
#include "ui/Window.h"

namespace alt {

class ShortcutEditWindow;

// Shortcut list manager (original Form\frmShortCutMan.pas), milestone M4.
//
// Native SysListView32 in report mode with four columns (关键字 / 显示名 /
// 参数类型 / 命令行). Every mutation (add/edit/delete/reorder/import) writes
// ShortCutList.txt immediately through ShortcutStore::Save(); closing the
// window never loses work, so X/Esc/取消 merely hide it (the window is created
// once and reused by App, like ConfigWindow/ParamWindow).
class ShortcutManagerWindow : public Window {
public:
    ShortcutManagerWindow(Settings& settings, ShortcutStore& store);
    ~ShortcutManagerWindow() override;

    bool CreateManagerWindow(HINSTANCE instance);

    // Rebuilds the list from store_.entries() and shows the window.
    void Open();

    // Called after every successful store_.Save() triggered by the manager.
    // App uses it to refresh the launcher's cached match list.
    void SetChangedHandler(std::function<void()> handler);

protected:
    LRESULT HandleMessage(UINT message, WPARAM wparam, LPARAM lparam) override;
    void OnCreate() override;
    void OnPaint() override;

private:
    // --- layout / resources ----------------------------------------------
    int S(int value) const;
    void CreateFonts();
    void DestroyResources();
    void CreateControls();
    HWND MakeControl(const wchar_t* class_name, const std::wstring& text,
                     DWORD style,
                     int x, int y, int width, int height, int id);
    void ApplyFont(HWND control) const;
    void PlaceOnCursorMonitor();
    void DrawHeader(HDC dc) const;
    void DrawListItem(const DRAWITEMSTRUCT& item);

    // --- list model -------------------------------------------------------
    void ReloadList(int select_row = -1);
    void UpdateButtonStates();
    int SelectedRow() const;
    void SelectRow(int row);
    static std::wstring ParamTypeLabel(ParamType type);
    static bool SameItem(const Shortcut& left, const Shortcut& right);
    bool IsDuplicate(const Shortcut& item, int ignore_index) const;

    // --- commands ---------------------------------------------------------
    void AddItem();
    void EditSelected();
    void DeleteSelected();
    void MoveSelected(int delta);
    void SaveAndNotify();
    void ShowContextMenu(POINT screen_position);

    // --- import / export --------------------------------------------------
    void ExportToFile();
    void ImportFromFile();
    void ReplaceFromImportedText(const std::string& text);
    static bool ReadFileBytes(const std::wstring& path, std::string* bytes);

    // --- drag reorder -----------------------------------------------------
    void BeginDrag(int row);
    void UpdateDragTarget(LPARAM lparam);
    void EndDrag();
    bool MoveRow(int from, int before);

    // --- focus navigation -------------------------------------------------
    void MoveFocus(bool backwards);

    static LRESULT CALLBACK ListSubclassProc(HWND hwnd, UINT message,
                                              WPARAM wparam, LPARAM lparam,
                                              UINT_PTR /*subclass_id*/,
                                              DWORD_PTR ref_data);
    static LRESULT CALLBACK ButtonSubclassProc(HWND hwnd, UINT message,
                                               WPARAM wparam, LPARAM lparam,
                                               UINT_PTR /*subclass_id*/,
                                               DWORD_PTR ref_data);

    // Reserved for the manager window geometry/column persistence that the
    // original kept in ALTRun.ini; Settings has no such fields yet.
    Settings& settings_;
    ShortcutStore& store_;
    std::function<void()> changed_handler_;
    std::unique_ptr<ShortcutEditWindow> edit_window_;

    int dpi_ = USER_DEFAULT_SCREEN_DPI;
    HFONT font_ = nullptr;
    HFONT title_font_ = nullptr;
    bool font_is_stock_ = false;
    HBRUSH background_brush_ = nullptr;
    HWND tooltip_ = nullptr;
    std::wstring up_tip_;
    std::wstring down_tip_;
    std::wstring list_tip_text_;

    HWND list_ = nullptr;
    HWND add_button_ = nullptr;
    HWND edit_button_ = nullptr;
    HWND delete_button_ = nullptr;
    HWND up_button_ = nullptr;
    HWND down_button_ = nullptr;
    HWND ok_button_ = nullptr;
    HWND export_button_ = nullptr;
    HWND import_button_ = nullptr;
    HWND hint_label_ = nullptr;

    bool dragging_ = false;
    int drag_from_ = -1;
    int drag_target_ = -1;
};

}  // namespace alt
