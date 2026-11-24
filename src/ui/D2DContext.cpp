#include "ui/D2DContext.h"

#include <d2d1helper.h>

#include <algorithm>

#include "core/Logger.h"

namespace alt {
namespace {

ID2D1Factory* g_d2d_factory = nullptr;
IDWriteFactory* g_dwrite_factory = nullptr;

}  // namespace

ID2D1Factory* D2DContext::Factory() {
    if (g_d2d_factory == nullptr) {
        D2D1_FACTORY_OPTIONS options = {};
        if (FAILED(::D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,
                                       __uuidof(ID2D1Factory), &options,
                                       reinterpret_cast<void**>(&g_d2d_factory)))) {
            Logger::Instance().Error("D2D1CreateFactory failed");
            g_d2d_factory = nullptr;
        }
    }
    return g_d2d_factory;
}

IDWriteFactory* D2DContext::WriteFactory() {
    if (g_dwrite_factory == nullptr) {
        if (FAILED(::DWriteCreateFactory(
                DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                reinterpret_cast<IUnknown**>(&g_dwrite_factory)))) {
            Logger::Instance().Error("DWriteCreateFactory failed");
            g_dwrite_factory = nullptr;
        }
    }
    return g_dwrite_factory;
}

D2DContext::~D2DContext() { Discard(); }

bool D2DContext::Initialize(HWND hwnd) {
    ID2D1Factory* factory = Factory();
    if (factory == nullptr) {
        return false;
    }

    RECT rc = {};
    ::GetClientRect(hwnd, &rc);

    const D2D1_SIZE_U size = D2D1::SizeU(
        static_cast<UINT>(rc.right - rc.left), static_cast<UINT>(rc.bottom - rc.top));

    const HRESULT hr = factory->CreateHwndRenderTarget(
        D2D1::RenderTargetProperties(),
        D2D1::HwndRenderTargetProperties(
            hwnd, size, D2D1_PRESENT_OPTIONS_IMMEDIATELY | D2D1_PRESENT_OPTIONS_RETAIN_CONTENTS),
        target_.ReleaseAndGetAddressOf());

    if (FAILED(hr)) {
        Logger::Instance().Error("CreateHwndRenderTarget failed");
        return false;
    }

    target_->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    target_->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE);
    target_->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White),
                                   brush_.ReleaseAndGetAddressOf());
    return true;
}

void D2DContext::Resize(UINT width, UINT height) {
    if (target_ && width > 0 && height > 0) {
        target_->Resize(D2D1::SizeU(width, height));
    }
}

void D2DContext::Discard() {
    brush_.Reset();
    target_.Reset();
}

bool D2DContext::BeginDraw() {
    if (!target_) {
        return false;
    }
    target_->BeginDraw();
    return true;
}

void D2DContext::EndDraw() {
    if (!target_) {
        return;
    }
    const HRESULT hr = target_->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET) {
        Discard();
    }
}

IDWriteTextFormat* D2DContext::CreateFormat(float size, DWRITE_FONT_WEIGHT weight,
                                            DWRITE_TEXT_ALIGNMENT alignment) {
    return CreateFormat(L"Segoe UI", size, weight, alignment);
}

IDWriteTextFormat* D2DContext::CreateFormat(const std::wstring& family, float size,
                                            DWRITE_FONT_WEIGHT weight,
                                            DWRITE_TEXT_ALIGNMENT alignment) {
    IDWriteFactory* write = WriteFactory();
    if (write == nullptr) {
        return nullptr;
    }
    IDWriteTextFormat* format = nullptr;
    const HRESULT hr = write->CreateTextFormat(
        family.empty() ? L"Segoe UI" : family.c_str(), nullptr, weight,
        DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, size,
        L"zh-cn",  // locale only affects shaping fallbacks, not the language UI
        &format);
    if (FAILED(hr)) {
        return nullptr;
    }
    format->SetTextAlignment(alignment);
    format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    return format;
}

ID2D1SolidColorBrush* D2DContext::Brush(const D2D1_COLOR_F& color) {
    if (!brush_ && target_) {
        target_->CreateSolidColorBrush(color, brush_.ReleaseAndGetAddressOf());
    }
    if (brush_) {
        brush_->SetColor(color);
        brush_->SetOpacity(1.0f);
    }
    return brush_.Get();
}

void D2DContext::FillRect(const D2D1_RECT_F& rect, const D2D1_COLOR_F& color) {
    if (!target_) {
        return;
    }
    if (ID2D1SolidColorBrush* brush = Brush(color)) {
        target_->FillRectangle(rect, brush);
    }
}

void D2DContext::FillRoundedRect(const D2D1_RECT_F& rect, float radius,
                                 const D2D1_COLOR_F& color) {
    if (!target_) {
        return;
    }
    if (ID2D1SolidColorBrush* brush = Brush(color)) {
        target_->FillRoundedRectangle(D2D1::RoundedRect(rect, radius, radius), brush);
    }
}

void D2DContext::DrawRoundedBorder(const D2D1_RECT_F& rect, float radius,
                                   const D2D1_COLOR_F& color, float width) {
    if (!target_) {
        return;
    }
    if (ID2D1SolidColorBrush* brush = Brush(color)) {
        target_->DrawRoundedRectangle(D2D1::RoundedRect(rect, radius, radius), brush,
                                      width);
    }
}

void D2DContext::DrawBitmap(ID2D1Bitmap* bitmap, const D2D1_RECT_F& dest,
                            float opacity) {
    if (!target_ || bitmap == nullptr) {
        return;
    }
    target_->DrawBitmap(bitmap, dest, opacity, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
}

void D2DContext::DrawTextBlock(const std::wstring& text, IDWriteTextFormat* format,
                               const D2D1_RECT_F& rect, const D2D1_COLOR_F& color) {
    DrawTextBlockRaw(text.c_str(), static_cast<UINT>(text.size()), format, rect, color);
}

void D2DContext::DrawTextEllipsis(const std::wstring& text,
                                  IDWriteTextFormat* format,
                                  const D2D1_RECT_F& rect,
                                  const D2D1_COLOR_F& color) {
    if (!target_ || format == nullptr || text.empty()) {
        return;
    }
    ID2D1SolidColorBrush* brush = Brush(color);
    IDWriteFactory* write = WriteFactory();
    if (brush == nullptr || write == nullptr) {
        return;
    }
    IDWriteTextLayout* layout = nullptr;
    if (FAILED(write->CreateTextLayout(
            text.c_str(), static_cast<UINT32>(text.size()), format,
            std::max(1.0f, rect.right - rect.left),
            std::max(1.0f, rect.bottom - rect.top), &layout))) {
        DrawTextBlockRaw(text.c_str(), static_cast<UINT>(text.size()), format,
                         rect, color);
        return;
    }
    layout->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
    IDWriteInlineObject* sign = nullptr;
    if (SUCCEEDED(write->CreateEllipsisTrimmingSign(format, &sign))) {
        DWRITE_TRIMMING trimming = {};
        trimming.granularity = DWRITE_TRIMMING_GRANULARITY_CHARACTER;
        trimming.delimiter = 0;
        trimming.delimiterCount = 0;
        layout->SetTrimming(&trimming, sign);
    }
    target_->DrawTextLayout(D2D1::Point2F(rect.left, rect.top), layout, brush,
                            D2D1_DRAW_TEXT_OPTIONS_CLIP);
    if (sign != nullptr) {
        sign->Release();
    }
    layout->Release();
}

void D2DContext::DrawTextBlockRaw(const wchar_t* text, UINT length,
                                  IDWriteTextFormat* format, const D2D1_RECT_F& rect,
                                  const D2D1_COLOR_F& color) {
    if (!target_ || format == nullptr || text == nullptr) {
        return;
    }
    if (ID2D1SolidColorBrush* brush = Brush(color)) {
        target_->DrawText(text, length, format, rect, brush,
                          D2D1_DRAW_TEXT_OPTIONS_CLIP);
    }
}

}  // namespace alt
