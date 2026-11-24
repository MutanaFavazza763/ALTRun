#pragma once

#include <windows.h>

#include <d2d1.h>
#include <dwrite.h>
#include <wrl/client.h>

#include <string>

namespace alt {

using Microsoft::WRL::ComPtr;

// Direct2D + DirectWrite wrapper for one HWND.
//
// Factories are process-wide (see Factory()/WriteFactory()); a context owns a
// hardware render target for a single window and is recreated automatically if
// the device is lost.
class D2DContext {
public:
    D2DContext() = default;
    ~D2DContext();

    D2DContext(const D2DContext&) = delete;
    D2DContext& operator=(const D2DContext&) = delete;

    bool Initialize(HWND hwnd);
    void Resize(UINT width, UINT height);
    void Discard();

    // Returns false when the frame should be skipped (device lost).
    bool BeginDraw();
    void EndDraw();

    ID2D1RenderTarget* target() const { return target_.Get(); }

    IDWriteTextFormat* CreateFormat(float size, DWRITE_FONT_WEIGHT weight,
                                    DWRITE_TEXT_ALIGNMENT alignment =
                                        DWRITE_TEXT_ALIGNMENT_LEADING);
    // Same as above but with an explicit font family (used by MainWindow for
    // the [GUI] TitleFont/KeywordFont/ListFont strings).
    IDWriteTextFormat* CreateFormat(const std::wstring& family, float size,
                                    DWRITE_FONT_WEIGHT weight,
                                    DWRITE_TEXT_ALIGNMENT alignment =
                                        DWRITE_TEXT_ALIGNMENT_LEADING);

    void FillRect(const D2D1_RECT_F& rect, const D2D1_COLOR_F& color);
    void FillRoundedRect(const D2D1_RECT_F& rect, float radius,
                         const D2D1_COLOR_F& color);
    void DrawRoundedBorder(const D2D1_RECT_F& rect, float radius,
                           const D2D1_COLOR_F& color, float width = 1.0f);
    // Stretches bitmap into dest; used for the skinned background.
    void DrawBitmap(ID2D1Bitmap* bitmap, const D2D1_RECT_F& dest,
                    float opacity = 1.0f);
    void DrawTextBlock(const std::wstring& text, IDWriteTextFormat* format,
                       const D2D1_RECT_F& rect, const D2D1_COLOR_F& color);
    void DrawTextBlockRaw(const wchar_t* text, UINT length, IDWriteTextFormat* format,
                          const D2D1_RECT_F& rect, const D2D1_COLOR_F& color);

    static ID2D1Factory* Factory();
    static IDWriteFactory* WriteFactory();

private:
    ID2D1SolidColorBrush* Brush(const D2D1_COLOR_F& color);

    ComPtr<ID2D1HwndRenderTarget> target_;
    ComPtr<ID2D1SolidColorBrush> brush_;
};

}  // namespace alt
