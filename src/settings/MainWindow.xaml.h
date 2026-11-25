#pragma once
#include "pch.h"
#include "MainWindow.g.h"
namespace winrt::ALTRunSettings::implementation {
struct MainWindow : MainWindowT<MainWindow> {
    MainWindow();

    void OnNavigationSelectionChanged(
        Microsoft::UI::Xaml::Controls::NavigationView const&,
        Microsoft::UI::Xaml::Controls::NavigationViewSelectionChangedEventArgs const&);
    void OnSaveClick(winrt::Windows::Foundation::IInspectable const&,
                     Microsoft::UI::Xaml::RoutedEventArgs const&);
    void OnCancelClick(winrt::Windows::Foundation::IInspectable const&,
                       Microsoft::UI::Xaml::RoutedEventArgs const&);
    void OnHotkeyKeyDown(winrt::Windows::Foundation::IInspectable const&,
                         Microsoft::UI::Xaml::Input::KeyRoutedEventArgs const&);

private:
    alt::Settings settings_;
    void LoadSettings();
    void SaveSettings();
    void ApplyLocalization();
    void ShowPage(std::wstring const& tag);
    void NotifyMainApp();
};
}

namespace winrt::ALTRunSettings::factory_implementation {
struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow> {};
}

