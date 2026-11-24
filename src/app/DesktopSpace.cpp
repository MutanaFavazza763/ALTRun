#include "app/DesktopSpace.h"

#include <shobjidl_core.h>

#include <string>

#include "core/Logger.h"

namespace alt {
namespace {

// {AA509086-5CA9-4C25-8F95-589D3C07B48A}: CLSID_VirtualDesktopManager. Written
// out locally so the launcher does not have to link uuid.lib for one GUID.
const CLSID kVirtualDesktopManager = {
    0xAA509086, 0x5CA9, 0x4C25, {0x8F, 0x95, 0x58, 0x9D, 0x3C, 0x07, 0xB4, 0x8A}};

}  // namespace

bool IsWindowOnCurrentDesktop(HWND hwnd) {
    if (hwnd == nullptr) {
        return false;
    }

    // wWinMain initialises STA, but this may run on a thread that did not.
    const HRESULT com = ::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const bool balance_com = SUCCEEDED(com);

    IVirtualDesktopManager* manager = nullptr;
    const HRESULT created = ::CoCreateInstance(
        kVirtualDesktopManager, nullptr, CLSCTX_INPROC_SERVER,
        __uuidof(IVirtualDesktopManager), reinterpret_cast<void**>(&manager));

    bool on_current = true;  // fail open: never rebuild on missing shell support
    if (SUCCEEDED(created) && manager != nullptr) {
        BOOL value = TRUE;
        const HRESULT queried =
            manager->IsWindowOnCurrentVirtualDesktop(hwnd, &value);
        if (SUCCEEDED(queried)) {
            on_current = value != FALSE;
        } else {
            Logger::Instance().Trace("virtual desktop query failed, assuming current");
        }
        manager->Release();
    } else {
        Logger::Instance().Trace(
            "virtual desktop manager unavailable, hr=" +
            std::to_string(static_cast<long>(created)));
    }

    if (balance_com) {
        ::CoUninitialize();
    }
    return on_current;
}

}  // namespace alt
