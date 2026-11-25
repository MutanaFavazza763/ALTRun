#include "pch.h"
#include "App.xaml.h"
#include "MainWindow.xaml.h"

namespace winrt { using namespace Microsoft::UI::Xaml; }

namespace {
HANDLE g_single_instance = nullptr;
bool g_already_running = false;
}

namespace winrt::ALTRunSettings::implementation {
App::App() {
    InitializeComponent();
    g_single_instance =
        ::CreateMutexW(nullptr, FALSE, L"ALTRun.Settings.SingleInstance.v1");
    g_already_running =
        g_single_instance != nullptr && ::GetLastError() == ERROR_ALREADY_EXISTS;
}

void App::OnLaunched(winrt::LaunchActivatedEventArgs const&) {
    if (g_already_running) {
        if (HWND existing = ::FindWindowW(nullptr, L"ALTRun Settings")) {
            ::ShowWindow(existing, SW_RESTORE);
            ::SetForegroundWindow(existing);
        }
        Exit();
        return;
    }
    window = winrt::make<MainWindow>();
    window.Activate();
}
}
