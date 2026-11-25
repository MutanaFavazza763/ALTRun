#include "pch.h"
#include "MainWindow.xaml.h"
#include <shellapi.h>
#if __has_include("MainWindow.g.cpp")
#include "MainWindow.g.cpp"
#endif

#include <algorithm>
#include <string>

#include "core/Localization.h"
#include "core/Logger.h"
#include "core/Text.h"
#include "ui/UiText.h"

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;

namespace winrt::ALTRunSettings::implementation {
namespace {

std::wstring W(char const* key) {
    return alt::Localization::Instance().Get(key);
}

hstring H(char const* key) {
    return hstring(W(key));
}

void SetToggleHeader(ToggleSwitch const& toggle, char const* key) {
    toggle.Header(box_value(H(key)));
}

void SetNumberHeader(NumberBox const& box, char const* key) {
    box.Header(box_value(H(key)));
}

void SetTextHeader(TextBox const& box, char const* key) {
    box.Header(box_value(H(key)));
}

std::wstring DirectoryOf(std::wstring path) {
    const size_t slash = path.find_last_of(L"\\/");
    if (slash == std::wstring::npos) {
        return {};
    }
    return path.substr(0, slash + 1);
}

std::wstring ExeDirectory() {
    wchar_t exe[MAX_PATH * 2] = {};
    ::GetModuleFileNameW(nullptr, exe, static_cast<DWORD>(std::size(exe)));
    return DirectoryOf(exe);
}

bool IsModifierKey(int vk) {
    return vk == VK_CONTROL || vk == VK_LCONTROL || vk == VK_RCONTROL ||
           vk == VK_MENU || vk == VK_LMENU || vk == VK_RMENU ||
           vk == VK_SHIFT || vk == VK_LSHIFT || vk == VK_RSHIFT ||
           vk == VK_LWIN || vk == VK_RWIN;
}

std::wstring KeyName(int vk) {
    if (vk >= 'A' && vk <= 'Z') {
        return std::wstring(1, static_cast<wchar_t>(vk));
    }
    if (vk >= '0' && vk <= '9') {
        return std::wstring(1, static_cast<wchar_t>(vk));
    }
    if (vk >= VK_F1 && vk <= VK_F24) {
        return L"F" + std::to_wstring(vk - VK_F1 + 1);
    }
    switch (vk) {
        case VK_PAUSE: return L"Pause";
        case VK_SPACE: return L"Space";
        case VK_RETURN: return L"Enter";
        case VK_TAB: return L"Tab";
        case VK_INSERT: return L"Insert";
        case VK_DELETE: return L"Delete";
        case VK_HOME: return L"Home";
        case VK_END: return L"End";
        case VK_PRIOR: return L"PageUp";
        case VK_NEXT: return L"PageDown";
        case VK_LEFT: return L"Left";
        case VK_RIGHT: return L"Right";
        case VK_UP: return L"Up";
        case VK_DOWN: return L"Down";
        case VK_SNAPSHOT: return L"PrintScreen";
        case VK_SCROLL: return L"ScrollLock";
        case VK_CAPITAL: return L"CapsLock";
        case VK_OEM_3: return L"`";
        case VK_OEM_MINUS: return L"-";
        case VK_OEM_PLUS: return L"=";
        case VK_OEM_COMMA: return L",";
        case VK_OEM_PERIOD: return L".";
        case VK_OEM_1: return L";";
        case VK_OEM_2: return L"/";
        case VK_OEM_5: return L"\\";
        case VK_OEM_6: return L"]";
        case VK_OEM_4: return L"[";
        case VK_OEM_7: return L"'";
        default: return {};
    }
}

std::string U8(hstring const& value) {
    return alt::ToUtf8(std::wstring(value));
}

}  // namespace

MainWindow::MainWindow() {
    InitializeComponent();
    Title(L"ALTRun Settings");

    ExtendsContentIntoTitleBar(true);
    SetTitleBar(AppTitleBar());
    try {
        SystemBackdrop(winrt::Microsoft::UI::Xaml::Media::MicaBackdrop());
    } catch (...) {
        // Mica is unavailable on older Windows builds; the theme background
        // still gives a usable Fluent surface.
    }

    AppWindow().Resize({1000, 720});
    try {
        auto area = winrt::Microsoft::UI::Windowing::DisplayArea::GetFromWindowId(
            AppWindow().Id(), winrt::Microsoft::UI::Windowing::DisplayAreaFallback::Primary);
        auto work = area.WorkArea();
        AppWindow().Move({work.X + (work.Width - 1000) / 2,
                          work.Y + (work.Height - 720) / 2});
    } catch (...) {
    }

    settings_.Load();
    alt::Localization::Instance().Load(settings_.lang);

    ApplyLocalization();
    LoadSettings();
    std::wstring initial_page = L"general";
    int argc = 0;
    if (wchar_t** argv = ::CommandLineToArgvW(::GetCommandLineW(), &argc)) {
        for (int i = 1; i + 1 < argc; ++i) {
            if (std::wstring(argv[i]) == L"--page") {
                initial_page = argv[i + 1];
                break;
            }
        }
        ::LocalFree(argv);
    }
    NavigationViewItem selected = NavGeneral();
    if (initial_page == L"hotkey") {
        selected = NavHotkey();
    } else if (initial_page == L"appearance") {
        selected = NavAppearance();
    } else if (initial_page == L"about" || initial_page == L"version") {
        initial_page = L"about";
        selected = NavAbout();
    } else {
        initial_page = L"general";
    }
    Nav().SelectedItem(selected);
    ShowPage(initial_page);
}

void MainWindow::ApplyLocalization() {
    TitleBarSubtitle().Text(L"Settings");

    NavGeneral().Content(box_value(H("resFluentSettingsGeneral")));
    NavHotkey().Content(box_value(H("ConfigForm.PageHotKey")));
    NavAppearance().Content(box_value(H("resFluentSettingsAppearance")));
    NavAbout().Content(box_value(H("resFluentSettingsAbout")));

    GeneralSearchTitle().Text(H("resFluentSettingsSearch"));
    GeneralSearchHint().Text(H("resSearchPageHint"));
    GeneralAdvancedTitle().Text(H("resFluentSettingsAdvanced"));

    SetToggleHeader(GeneralRegex(), "ConfigForm.ConfigList_2");
    SetToggleHeader(GeneralMatchAnywhere(), "ConfigForm.ConfigList_3");
    SetToggleHeader(GeneralNumberKey(), "ConfigForm.ConfigList_4");
    SetToggleHeader(GeneralIndexFrom0(), "ConfigForm.ConfigList_5");
    SetToggleHeader(GeneralRememberFavourite(), "ConfigForm.ConfigList_6");
    SetToggleHeader(GeneralShowTopTen(), "ConfigForm.ConfigList_10");
    SetToggleHeader(GeneralExecuteIfOnlyOne(), "ConfigForm.ConfigList_19");
    SetToggleHeader(GeneralExitWhenExecute(), "ConfigForm.ConfigList_12");
    SetToggleHeader(GeneralShowCommandLine(), "ConfigForm.ConfigList_8");
    SetToggleHeader(GeneralShowOperationHint(), "ConfigForm.ConfigList_7");
    SetToggleHeader(GeneralAutoRun(), "ConfigForm.ConfigList_0");
    SetToggleHeader(GeneralAddToSendTo(), "ConfigForm.ConfigList_1");
    SetToggleHeader(GeneralShowMeWhenStart(), "ConfigForm.ConfigList_14");

    SetNumberHeader(GeneralHideDelay(), "resHideDelay");
    GeneralHideDelayHint().Text(H("resHideDelayHint"));
    SetNumberHeader(GeneralParamHistoryLimit(), "resParamHistoryLimit");
    GeneralParamHistoryHint().Text(H("resParamHistoryLimitHint"));
    GeneralHint().Text(H("resFluentSettingsAdvanced"));

    HotkeyCardTitle().Text(H("resFluentSettingsHotkeys"));
    SetTextHeader(Hotkey1Box(), "ConfigForm.LblHotKey1");
    SetTextHeader(Hotkey2Box(), "ConfigForm.LblHotKey2");
    SetTextHeader(LastItemHotkeyBox(), "resLastItemHotKey");
    Hotkey1Box().PlaceholderText(H("resHotkeyCue"));
    Hotkey2Box().PlaceholderText(H("resHotkeyCue"));
    LastItemHotkeyBox().PlaceholderText(H("resHotkeyCue"));
    HotkeyHint1().Text(H("resHotkeyHint1"));
    HotkeyHint2().Text(H("resHotkeyHint2"));
    HotkeyHint3().Text(H("resHotkeyHint3"));

    AppearanceCardTitle().Text(H("resAppearanceCardTitle"));
    SetNumberHeader(FormWidthBox(), "ConfigForm.lblFormWidth");
    SetNumberHeader(AlphaBox(), "ConfigForm.LblFormAlpha");
    SetNumberHeader(RadiusBox(), "ConfigForm.lblRoundBorderRadius");
    LanguageCombo().Header(box_value(H("ConfigForm.LblLanguage")));
    SetTextHeader(BackgroundBox(), "ConfigForm.LblBackGroundImage");

    DisplayCardTitle().Text(H("resFluentSettingsDisplay"));
    SetTextHeader(ListFormatBox(), "resListFormat");
    ListFormatHint().Text(H("resListFormatHint"));
    OptionsCardTitle().Text(H("resFluentSettingsOptions"));
    SetToggleHeader(ShowSkinToggle(), "ConfigForm.ConfigList_13");
    SetToggleHeader(ShowTrayIconToggle(), "ConfigForm.ConfigList_15");
    SetToggleHeader(PlayPopupNotifyToggle(), "ConfigForm.ConfigList_11");
    SetToggleHeader(ShowStartNotificationToggle(), "ConfigForm.ConfigList_9");
    AppearanceHint().Text(H("resAppearanceHint"));

    AboutCardTitle().Text(H("resAboutCardTitle"));
    AboutTrace().Header(box_value(H("resTraceEnable")));
    AboutRelatedFilesTitle().Text(H("resRelatedFiles"));
    AboutHint().Text(H("resTraceEnableHint"));

    CancelButton().Content(box_value(H("Common.BtnCancel")));
    SaveButton().Content(box_value(H("Common.BtnOK")));
}

void MainWindow::LoadSettings() {
    GeneralRegex().IsOn(settings_.regex_mode);
    GeneralMatchAnywhere().IsOn(settings_.match_anywhere);
    GeneralNumberKey().IsOn(settings_.number_key);
    GeneralIndexFrom0().IsOn(settings_.index_from_0_to_9);
    GeneralRememberFavourite().IsOn(settings_.remember_favourite_match);
    GeneralShowTopTen().IsOn(settings_.show_top_ten);
    GeneralExecuteIfOnlyOne().IsOn(settings_.execute_if_only_one);
    GeneralExitWhenExecute().IsOn(settings_.exit_when_execute);
    GeneralShowCommandLine().IsOn(settings_.show_command_line);
    GeneralShowOperationHint().IsOn(settings_.show_operation_hint);
    GeneralAutoRun().IsOn(settings_.auto_run);
    GeneralAddToSendTo().IsOn(settings_.add_to_send_to);
    GeneralShowMeWhenStart().IsOn(settings_.show_me_when_start);

    GeneralHideDelay().Value(static_cast<double>(settings_.hide_delay));
    GeneralHideDelay().Minimum(0);
    GeneralHideDelay().Maximum(3600);
    GeneralParamHistoryLimit().Value(static_cast<double>(settings_.param_history_limit));
    GeneralParamHistoryLimit().Minimum(0);
    GeneralParamHistoryLimit().Maximum(1000);

    Hotkey1Box().Text(hstring(alt::ToWide(settings_.hotkey1)));
    Hotkey2Box().Text(hstring(alt::ToWide(settings_.hotkey2)));
    LastItemHotkeyBox().Text(hstring(alt::ToWide(settings_.last_item_hotkey)));

    FormWidthBox().Value(static_cast<double>(settings_.form_width));
    FormWidthBox().Minimum(200);
    FormWidthBox().Maximum(800);
    AlphaBox().Value(static_cast<double>(settings_.alpha));
    AlphaBox().Minimum(0);
    AlphaBox().Maximum(255);
    RadiusBox().Value(static_cast<double>(settings_.round_border_radius));
    RadiusBox().Minimum(0);
    RadiusBox().Maximum(200);

    BackgroundBox().Text(hstring(alt::ToWide(settings_.bg_file_name)));
    ListFormatBox().Text(hstring(alt::ToWide(settings_.list_format)));

    ShowSkinToggle().IsOn(settings_.show_skin);
    ShowTrayIconToggle().IsOn(settings_.show_tray_icon);
    PlayPopupNotifyToggle().IsOn(settings_.play_popup_notify);
    ShowStartNotificationToggle().IsOn(settings_.show_start_notification);

    int language_index = 2;
    const std::string normalized =
        alt::Localization::NormalizeLanguage(settings_.lang);
    int item_index = 0;
    for (auto const& item : LanguageCombo().Items()) {
        auto combo_item = item.try_as<ComboBoxItem>();
        if (!combo_item) {
            ++item_index;
            continue;
        }
        auto tag = combo_item.Tag();
        if (tag && winrt::unbox_value<hstring>(tag) == hstring(alt::ToWide(normalized))) {
            language_index = item_index;
            break;
        }
        ++item_index;
    }
    LanguageCombo().SelectedIndex(language_index);

    AboutVersion().Text(hstring(std::wstring(L"2.1.0")));
    AboutIniPath().Text(hstring(W("resAboutIniPath") + settings_.ini_path));
    AboutRunCount().Text(hstring(alt::LocArgInt(
        W("resAboutRunCount"), settings_.shortcut_run_count)));

    const std::wstring dir = settings_.ini_path.empty()
                                 ? ExeDirectory()
                                 : DirectoryOf(settings_.ini_path);
    AboutParamHistory().Text(hstring(W("resParamHistoryFile") +
                                      (dir.empty() ? ExeDirectory() : dir) +
                                      L"ParamHistory.txt"));
    AboutShortcutList().Text(hstring(W("resShortcutListFile") +
                                     (dir.empty() ? ExeDirectory() : dir) +
                                     L"ShortCutList.txt"));
    AboutFavouriteList().Text(hstring(W("resFavoriteListFile") +
                                      (dir.empty() ? ExeDirectory() : dir) +
                                      L"FavoriteList.txt"));
    AboutTrace().IsOn(settings_.trace_enabled);
}

void MainWindow::SaveSettings() {
    settings_.regex_mode = GeneralRegex().IsOn();
    settings_.match_anywhere = GeneralMatchAnywhere().IsOn();
    settings_.number_key = GeneralNumberKey().IsOn();
    settings_.index_from_0_to_9 = GeneralIndexFrom0().IsOn();
    settings_.remember_favourite_match = GeneralRememberFavourite().IsOn();
    settings_.show_top_ten = GeneralShowTopTen().IsOn();
    settings_.execute_if_only_one = GeneralExecuteIfOnlyOne().IsOn();
    settings_.exit_when_execute = GeneralExitWhenExecute().IsOn();
    settings_.show_command_line = GeneralShowCommandLine().IsOn();
    settings_.show_operation_hint = GeneralShowOperationHint().IsOn();
    settings_.auto_run = GeneralAutoRun().IsOn();
    settings_.add_to_send_to = GeneralAddToSendTo().IsOn();
    settings_.show_me_when_start = GeneralShowMeWhenStart().IsOn();

    settings_.hide_delay = std::clamp(
        static_cast<int>(GeneralHideDelay().Value()), 0, 3600);
    settings_.param_history_limit = std::clamp(
        static_cast<int>(GeneralParamHistoryLimit().Value()), 0, 1000);

    settings_.hotkey1 = alt::TrimAscii(U8(Hotkey1Box().Text()));
    settings_.hotkey2 = alt::TrimAscii(U8(Hotkey2Box().Text()));
    settings_.last_item_hotkey = alt::TrimAscii(U8(LastItemHotkeyBox().Text()));

    settings_.form_width = std::clamp(
        static_cast<int>(FormWidthBox().Value()), 200, 800);
    settings_.alpha = std::clamp(
        static_cast<int>(AlphaBox().Value()), 0, 255);
    settings_.round_border_radius = std::clamp(
        static_cast<int>(RadiusBox().Value()), 0, 200);

    settings_.bg_file_name = alt::TrimAscii(U8(BackgroundBox().Text()));
    settings_.list_format = alt::TrimAscii(U8(ListFormatBox().Text()));

    settings_.show_skin = ShowSkinToggle().IsOn();
    settings_.show_tray_icon = ShowTrayIconToggle().IsOn();
    settings_.play_popup_notify = PlayPopupNotifyToggle().IsOn();
    settings_.show_start_notification = ShowStartNotificationToggle().IsOn();

    if (auto item = LanguageCombo().SelectedItem().try_as<ComboBoxItem>()) {
        if (auto tag = item.Tag()) {
            settings_.lang = alt::Localization::NormalizeLanguage(
                winrt::to_string(winrt::unbox_value<hstring>(tag)));
        }
    }

    settings_.trace_enabled = AboutTrace().IsOn();

    if (!settings_.Save()) {
        alt::Logger::Instance().Warn("WinUI settings: saving ALTRun.ini failed");
    }
    alt::Localization::Instance().Load(settings_.lang);
    NotifyMainApp();
}

void MainWindow::NotifyMainApp() {
    if (HWND main = ::FindWindowW(L"ALTRun.MainWindow.v2", nullptr)) {
        ::PostMessageW(main, WM_APP + 30, 0, 0);
    }
}

void MainWindow::ShowPage(std::wstring const& tag) {
    const Visibility visible = Visibility::Visible;
    const Visibility hidden = Visibility::Collapsed;
    GeneralPage().Visibility(tag == L"general" ? visible : hidden);
    HotkeyPage().Visibility(tag == L"hotkey" ? visible : hidden);
    AppearancePage().Visibility(tag == L"appearance" ? visible : hidden);
    AboutPage().Visibility(tag == L"about" ? visible : hidden);

    if (tag == L"general") {
        PageTitle().Text(H("resFluentSettingsGeneral"));
        PageSubtitle().Text(H("resGeneralPageHint"));
    } else if (tag == L"hotkey") {
        PageTitle().Text(H("ConfigForm.PageHotKey"));
        PageSubtitle().Text(H("resHotkeyPageHint"));
    } else if (tag == L"appearance") {
        PageTitle().Text(H("resFluentSettingsAppearance"));
        PageSubtitle().Text(H("resAppearanceHint"));
    } else {
        PageTitle().Text(H("resFluentSettingsAbout"));
        PageSubtitle().Text(H("resAboutPageHint"));
    }
}

void MainWindow::OnNavigationSelectionChanged(
    NavigationView const&, NavigationViewSelectionChangedEventArgs const& args) {
    auto item = args.SelectedItem().try_as<NavigationViewItem>();
    if (!item) {
        return;
    }
    auto tag = item.Tag();
    if (!tag) {
        return;
    }
    ShowPage(std::wstring(winrt::unbox_value<hstring>(tag)));
}

void MainWindow::OnSaveClick(IInspectable const&, RoutedEventArgs const&) {
    SaveSettings();
    Close();
}

void MainWindow::OnCancelClick(IInspectable const&, RoutedEventArgs const&) {
    Close();
}

void MainWindow::OnHotkeyKeyDown(IInspectable const& sender,
                                Microsoft::UI::Xaml::Input::KeyRoutedEventArgs const& args) {
    auto box = sender.try_as<TextBox>();
    if (!box) {
        return;
    }

    const int vk = static_cast<int>(args.Key());
    if (vk == VK_ESCAPE || vk == VK_BACK || vk == VK_DELETE) {
        box.Text(L"");
        args.Handled(true);
        return;
    }
    if (IsModifierKey(vk)) {
        args.Handled(true);
        return;
    }

    std::wstring text;
    if ((::GetKeyState(VK_CONTROL) & 0x8000) != 0) {
        text += L"Ctrl + ";
    }
    if ((::GetKeyState(VK_MENU) & 0x8000) != 0) {
        text += L"Alt + ";
    }
    if ((::GetKeyState(VK_SHIFT) & 0x8000) != 0) {
        text += L"Shift + ";
    }
    if ((::GetKeyState(VK_LWIN) & 0x8000) != 0 ||
        (::GetKeyState(VK_RWIN) & 0x8000) != 0) {
        text += L"Win + ";
    }
    std::wstring key = KeyName(vk);
    if (key.empty()) {
        args.Handled(true);
        return;
    }
    text += key;
    box.Text(hstring(text));
    args.Handled(true);
}

}  // namespace winrt::ALTRunSettings::implementation



