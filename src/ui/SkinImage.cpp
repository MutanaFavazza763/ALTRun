#include "ui/SkinImage.h"

#include <d2d1helper.h>

#include <cstdio>
#include <string>

#include "core/Logger.h"
#include "core/Text.h"

namespace alt {
namespace {

std::string HResultText(HRESULT result) {
    char buffer[16] = {};
    std::snprintf(buffer, sizeof(buffer), "0x%08lX",
                  static_cast<unsigned long>(result));
    return buffer;
}

void LogFailure(const char* what, HRESULT result, const std::wstring& path) {
    Logger::Instance().Warn(std::string("skin: ") + what + " failed (hr=" +
                            HResultText(result) + "), path=" + ToUtf8(path));
}

}  // namespace

SkinImage::~SkinImage() { Release(); }

bool SkinImage::Load(ID2D1RenderTarget* target, const std::wstring& path) {
    Release();
    if (target == nullptr || path.empty()) {
        return false;
    }

    if (!factory_) {
        const HRESULT created = ::CoCreateInstance(
            CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
            IID_PPV_ARGS(factory_.ReleaseAndGetAddressOf()));
        if (FAILED(created)) {
            LogFailure("CoCreateInstance(WICImagingFactory)", created, path);
            return false;
        }
    }

    ComPtr<IWICBitmapDecoder> decoder;
    HRESULT result = factory_->CreateDecoderFromFilename(
        path.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad,
        decoder.ReleaseAndGetAddressOf());
    if (FAILED(result)) {
        LogFailure("CreateDecoderFromFilename", result, path);
        return false;
    }

    ComPtr<IWICBitmapFrameDecode> frame;
    result = decoder->GetFrame(0, frame.ReleaseAndGetAddressOf());
    if (FAILED(result)) {
        LogFailure("GetFrame", result, path);
        return false;
    }

    // 32bppPBGRA is premultiplied and directly consumable by Direct2D.
    ComPtr<IWICFormatConverter> converter;
    result = factory_->CreateFormatConverter(converter.ReleaseAndGetAddressOf());
    if (SUCCEEDED(result)) {
        result = converter->Initialize(
            frame.Get(), GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone,
            nullptr, 0.0, WICBitmapPaletteTypeCustom);
    }
    if (FAILED(result)) {
        LogFailure("WIC format conversion", result, path);
        return false;
    }

    result = target->CreateBitmapFromWicBitmap(
        converter.Get(), nullptr, bitmap_.ReleaseAndGetAddressOf());
    if (FAILED(result)) {
        LogFailure("CreateBitmapFromWicBitmap", result, path);
        return false;
    }

    const D2D1_SIZE_F size = bitmap_->GetSize();
    Logger::Instance().Info("skin: loaded " + ToUtf8(path) + " (" +
                            std::to_string(static_cast<int>(size.width)) + "x" +
                            std::to_string(static_cast<int>(size.height)) + ")");
    return true;
}

void SkinImage::Release() { bitmap_.Reset(); }

D2D1_SIZE_F SkinImage::size() const {
    return bitmap_ ? bitmap_->GetSize() : D2D1::SizeF(0.0f, 0.0f);
}

}  // namespace alt
