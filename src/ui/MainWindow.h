#pragma once

#include <windows.h>

#include <dwrite.h>
#include <wrl/client.h>

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "app/Settings.h"
#include "core/Matcher.h"
#include "core/ParamHistory.h"
#include "core/ShortcutStore.h"
#include "ui/D2DContext.h"
#include "ui/SkinImage.h"
#include "ui/Window.h"

namespace alt {

class ParamWindow;

// The launcher window: borderless, topmost, centered on the cursor's monitor.
//
// Behaviour mirrors the original frmALTRun.pas:
//   * single edit line, results in a custom drawn list (max 10 rows by default)
//   * keyboard driven selection and execution (Enter, digits, arrows, Esc)
//   * ListFormat based row text plus the numeric/parameter prefixes
class MainWindow : public Window {
public:
    MainWindow(Settings& settings, ShortcutStore& store, ParamHistory& param_history);
    ~MainWindow() override;

    bool CreateMain(HINSTANCE instance);

    void ToggleLauncher();
    void ShowLauncher();
    void HideLauncher();

    // Recomputes |matches_| from the current query.
    void RefreshMatches();

    // Executes the most recently used item (LastItemHotKey, default ALT+L).
    void LaunchLatest();

    // Applies changed settings (alpha, width, timers) to the live window.
    void ApplySettings();

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
    // (Re)creates the DirectWrite formats from Settings' font strings; also
    // called by ApplySettings after the settings dialog wrote new fonts.
    void CreateTextFormats();
    // Loads/releases the skinned background for settings_.show_skin /
    // settings_.bg_file_name (silent fallback when the file is missing).
    void ReloadSkin();
    std::wstring SkinPath() const;
    void DrawFrame();
    void DrawRows();
    void DrawFooter();
    float MeasureWidth(const std::wstring& text, IDWriteTextFormat* format) const;

    // --- behaviour --------------------------------------------------------
    std::string FormatRow(size_t row) const;
    std::string NumberPrefixFor(size_t row, const ShortcutEntry& entry) const;
    void LaunchRow(size_t row);
    // Prompts for a parameter when needed, then executes.
    void LaunchEntry(size_t entry_index, bool count_freq = true);
    // Performs the actual launch (no prompt).
    void ExecuteEntry(size_t entry_index, std::string param, bool count_freq);
    // Ctrl+L: show the recent list without filtering.
    void ShowRecentList();
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
    ParamHistory& param_history_;

    D2DContext d2d_;
    SkinImage skin_;
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
    // True while the recent list (Ctrl+L) is displayed instead of matches.
    bool showing_recent_ = false;
    POINT drag_origin_ = {};

    std::unique_ptr<ParamWindow> param_window_;

    std::function<void()> exit_handler_;
};

}  // namespace alt
