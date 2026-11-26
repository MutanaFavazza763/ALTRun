#pragma once

#include <windows.h>

#include <dwrite.h>
#include <wrl/client.h>

#include <functional>
#include <string>
#include <vector>

#include "app/Settings.h"
#include "core/ParamHistory.h"
#include "core/Shortcut.h"
#include "ui/D2DContext.h"
#include "ui/Window.h"

namespace alt {

// Parameter prompt shown before launching an item whose ParamType is not None
// (original Form\frmParam.pas):
//   * topmost popup with a single line edit and the global history list
//   * Enter confirms, Esc clears then cancels (two stage)
//   * idle for HideDelay seconds cancels automatically
class ParamWindow : public Window {
public:
    using ConfirmHandler = std::function<void(std::string param)>;

    ParamWindow(Settings& settings, ParamHistory& history);
    ~ParamWindow() override;

    bool CreateParamWindow(HINSTANCE instance);

    // Shows the prompt for |item|; |on_confirm| runs only when the user
    // confirms (Enter or the OK button).
    void Open(const Shortcut& item, ConfirmHandler on_confirm);

    void Cancel();

protected:
    LRESULT HandleMessage(UINT message, WPARAM wparam, LPARAM lparam) override;
    void OnCreate() override;
    void OnPaint() override;

private:
    void Relayout();
    void DrawFrame();
    void DrawHistory();
    void Confirm();
    void HandleEscape();
    void MoveHistory(int delta);
    void ApplyHistorySelection();
    void ResetIdleTimer();
    void PlaceOnCursorMonitor();

    std::wstring EditText() const;
    void SetEditText(const std::wstring& text);
    D2D1_RECT_F EditRect() const;
    D2D1_RECT_F OkButtonRect() const;
    D2D1_RECT_F HistoryRowRect(size_t row) const;
    size_t VisibleHistoryRows() const;

    static LRESULT CALLBACK EditSubclassProc(HWND hwnd, UINT message, WPARAM wparam,
                                            LPARAM lparam, UINT_PTR subclass_id,
                                            DWORD_PTR ref_data);

    Settings& settings_;
    ParamHistory& history_;

    D2DContext d2d_;
    ComPtr<IDWriteTextFormat> title_format_;
    ComPtr<IDWriteTextFormat> row_format_;
    ComPtr<IDWriteTextFormat> hint_format_;

    HWND edit_ = nullptr;
    HBRUSH edit_brush_ = nullptr;
    std::vector<ParamHistoryItem> shown_;
    int selected_ = -1;
    bool ok_hover_ = false;
    bool ok_pressed_ = false;
    std::wstring item_label_;
    ConfirmHandler on_confirm_;
};

}  // namespace alt
