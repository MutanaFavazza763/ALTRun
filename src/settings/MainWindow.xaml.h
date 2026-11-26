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
    void OnLanguageItemClick(winrt::Windows::Foundation::IInspectable const&,
                             Microsoft::UI::Xaml::RoutedEventArgs const&);
    void OnThemeItemClick(winrt::Windows::Foundation::IInspectable const&,
                          Microsoft::UI::Xaml::RoutedEventArgs const&);
    void OnPickSkinClick(winrt::Windows::Foundation::IInspectable const&,
                         Microsoft::UI::Xaml::RoutedEventArgs const&);
    void OnCopyPathClick(winrt::Windows::Foundation::IInspectable const&,
                         Microsoft::UI::Xaml::RoutedEventArgs const&);

private:
    alt::Settings settings_;
    int language_index_ = 2;
    std::string language_value_ = "en_us";
    std::string theme_value_ = "system";
    void LoadSettings();
    void SaveSettings();
    void ApplyLocalization();
    void UpdateLanguageButton();
    void ShowPage(std::wstring const& tag);
    void NotifyMainApp();
};
}

namespace winrt::ALTRunSettings::factory_implementation {
struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow> {};
}

