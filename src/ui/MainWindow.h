#pragma once

#include <windows.h>

#include <dwrite.h>
#include <wrl/client.h>

#include <functional>
#include <string>
#include <vector>

#include "app/Settings.h"
#include "core/Matcher.h"
#include "core/ShortcutStore.h"
#include "ui/D2DContext.h"
#include "ui/Window.h"

namespace alt {

// The launcher window: borderless, topmost, centered on the cursor's monitor.
//
// Behaviour mirrors the original frmALTRun.pas:
//   * single edit line, results in a custom drawn list (max 10 rows by default)
//   * keyboard driven selection and execution (Enter, digits, arrows, Esc)
//   * ListFormat based row text plus the numeric/parameter prefixes
class MainWindow : public Window {
public:
    MainWindow(Settings& settings, ShortcutStore& store);
    ~MainWindow() override;

    bool CreateMain(HINSTANCE instance);

    void ToggleLauncher();
    void ShowLauncher();
    void HideLauncher();

    // Recomputes |matches_| from the current query.
    void RefreshMatches();

    void SetExitHandler(std::function<void()> handler);

protected:
    LRESULT HandleMessage(UINT message, WPARAM wparam, LPARAM lparam) override;
    void OnCreate() override;
    void OnPaint() override;

private:
    // --- layout -----------------------------------------------------------
    void Relayout();
    D2D1_RECT_F InputRect() const;
    D2D1_RECT_F RowRect(size_t row) const;
    size_t VisibleRows() const;
    float ContentHeight() const;

    // --- painting ---------------------------------------------------------
    void DrawFrame();
    void DrawRows();
    void DrawFooter();
    float MeasureWidth(const std::wstring& text, IDWriteTextFormat* format) const;

    // --- behaviour --------------------------------------------------------
    std::string FormatRow(size_t row) const;
    std::string NumberPrefixFor(size_t row, const ShortcutEntry& entry) const;
    void LaunchRow(size_t row);
    void LaunchEntry(size_t entry_index);
    void MoveSelection(int delta);
    void AppendQuery(const std::wstring& text);
    void Backspace();
    void ClearQuery();
    void PasteFromClipboard();
    void PlaceOnCursorMonitor();
    void UpdateWindowAlpha();
    // Restarts the idle auto-hide timer (HideDelay seconds, original
    // behaviour: the popup disappears when left untouched).
    void ResetHideTimer();

    // --- message helpers --------------------------------------------------
    void HandleKeyDown(WPARAM vk);
    void HandleChar(WPARAM ch);
    void HandleImeComposition(LPARAM lparam);
    void PositionImeWindow();

    Settings& settings_;
    ShortcutStore& store_;

    D2DContext d2d_;
    ComPtr<IDWriteTextFormat> title_format_;
    ComPtr<IDWriteTextFormat> query_format_;
    ComPtr<IDWriteTextFormat> row_format_;
    ComPtr<IDWriteTextFormat> row_bold_format_;
    ComPtr<IDWriteTextFormat> hint_format_;
    ComPtr<IDWriteTextFormat> mono_format_;

    std::vector<MatchResult> matches_;
    std::string query_;
    int selected_ = 0;
    int hover_ = -1;

    std::wstring ime_composition_;
    bool caret_visible_ = true;
    bool dragging_ = false;
    POINT drag_origin_ = {};

    std::function<void()> exit_handler_;
};

}  // namespace alt
