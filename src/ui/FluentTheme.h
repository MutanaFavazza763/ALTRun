#pragma once

#include <windows.h>

#include <string>

namespace alt {
namespace fluent {

// Fluent / WinUI-inspired palette used by all ALTRun windows.  The reference
// is ZufyUI's light theme: white surfaces, #0078D6 accent, soft blue-tinted
// Mica-like background, 8-12 DIP corner radii and Segoe UI Variable.
const COLORREF kBackgroundTop = RGB(242, 247, 254);
const COLORREF kBackgroundBottom = RGB(228, 237, 248);
const COLORREF kSurface = RGB(255, 255, 255);
const COLORREF kSurfaceAlt = RGB(246, 248, 251);
const COLORREF kBorder = RGB(214, 224, 236);
const COLORREF kDivider = RGB(229, 235, 242);
const COLORREF kText = RGB(24, 28, 33);
const COLORREF kTextSecondary = RGB(80, 88, 99);
const COLORREF kTextDisabled = RGB(108, 116, 126);
const COLORREF kAccent = RGB(0, 120, 214);
const COLORREF kAccentHover = RGB(26, 140, 228);
const COLORREF kAccentPressed = RGB(0, 90, 170);
const COLORREF kAccentSoft = RGB(214, 233, 255);
const COLORREF kSelection = RGB(204, 228, 255);
const COLORREF kDanger = RGB(196, 43, 28);
const COLORREF kShadow = RGB(198, 211, 226);

int Scale(HWND window, int value);
HFONT CreateFont(HWND window, int point_size, bool semibold = false);
HICON AppIcon();

// Background / chrome ------------------------------------------------------
void FillBackground(HDC dc, const RECT& client);
void FillRounded(HDC dc, const RECT& rect, int radius, COLORREF color);
void StrokeRounded(HDC dc, const RECT& rect, int radius, COLORREF color,
                   int width = 1);
void DrawCard(HDC dc, const RECT& rect, int radius = 12);
void DrawTitleBar(HDC dc, HWND window, const std::wstring& title, HFONT title_font,
                  int title_height);
RECT CloseButtonRect(HWND window, int title_height);
// Handles title-bar dragging, close-button hover and close click.  Returns
// -1 when the message should continue through the window's normal handler.
LRESULT HandleChromeMessage(HWND window, UINT message, WPARAM wparam,
                            LPARAM lparam, int title_height);

// Navigation / controls ---------------------------------------------------
void DrawNavItem(HDC dc, const RECT& rect, const std::wstring& text, HFONT font,
                 bool selected, bool hover);
void DrawButton(HDC dc, const RECT& rect, const std::wstring& text, HFONT font,
                bool primary, bool hover, bool pressed, bool focused,
                bool enabled = true);
void DrawToggle(HDC dc, const RECT& rect, const std::wstring& text, HFONT font,
                bool on, bool hover, bool focused);
void DrawRadio(HDC dc, const RECT& rect, const std::wstring& text, HFONT font,
               bool checked, bool hover, bool focused);
void DrawSeparator(HDC dc, const RECT& rect);

// Child controls.  They draw themselves, keep their own check state (so the
// existing BM_SETCHECK/BM_GETCHECK value-flow keeps working) and post a normal
// BN_CLICKED WM_COMMAND to their parent.
HWND CreateToggle(HWND parent, HINSTANCE instance, int id, const std::wstring& text,
                  HFONT font, int width, int height, int dpi);
HWND CreateButton(HWND parent, HINSTANCE instance, int id, const std::wstring& text,
                  HFONT font, int width, int height, int dpi, bool primary);
HWND CreateRadio(HWND parent, HINSTANCE instance, int id, const std::wstring& text,
                 HFONT font, int width, int height, int dpi);

}  // namespace fluent
}  // namespace alt
