#include <windows.h>

#include "app/App.h"

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE /*prev_instance*/,
                    PWSTR /*command_line*/, int /*show_command*/) {
    // COINIT_APARTMENTTHREADED is required by the shell APIs (tray icon,
    // ShellExecuteEx) and by IME.
    if (FAILED(::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED |
                                             COINIT_DISABLE_OLE1DDE))) {
        return 1;
    }

    ::SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    alt::App app(instance);
    const int result = app.Run();

    ::CoUninitialize();
    return result;
}
