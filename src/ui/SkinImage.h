#pragma once

#include <windows.h>

#include <d2d1.h>
#include <wincodec.h>
#include <wrl/client.h>

#include <string>

namespace alt {

using Microsoft::WRL::ComPtr;

// WIC decoded skin bitmap (original [GUI] BGFileName, default BG.jpg).
//
// The image is decoded with IWICImagingFactory and converted to a
// device-bound ID2D1Bitmap through ID2D1RenderTarget::CreateBitmapFromWicBitmap,
// so the caller must reload it whenever the render target is recreated (the
// bitmap belongs to the old device).  Direct2D/WIC are system components, no
// third-party dependency is involved.
class SkinImage {
public:
    SkinImage() = default;
    ~SkinImage();

    SkinImage(const SkinImage&) = delete;
    SkinImage& operator=(const SkinImage&) = delete;

    // Decodes path (PNG/JPG/BMP/GIF via WIC) into a bitmap for target.
    // Returns false and logs the reason on failure.
    bool Load(ID2D1RenderTarget* target, const std::wstring& path);
    void Release();

    bool valid() const { return bitmap_ != nullptr; }
    ID2D1Bitmap* bitmap() const { return bitmap_.Get(); }
    D2D1_SIZE_F size() const;

private:
    ComPtr<IWICImagingFactory> factory_;
    ComPtr<ID2D1Bitmap> bitmap_;
};

}  // namespace alt
