#include "pch.h"
#include "MainWindow.xaml.h"
#include <shellapi.h>
#include <commdlg.h>
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "shell32.lib")
#if __has_include("MainWindow.g.cpp")
#include "MainWindow.g.cpp"
#endif

#include <algorithm>
#include <string>

#include <winrt/Windows.ApplicationModel.DataTransfer.h>

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

void SetToggleLabel(TextBlock const& label, char const* key) {
    label.Text(H(key));
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

// UI-facing paths stay portable: files next to the executable are shown as
// ".\name", and paths below the current profile as "%USERPROFILE%\...".
// The full absolute path remains available in the tooltip and on copy.
std::wstring DisplayPath(const std::wstring& path) {
    if (path.empty()) {
        return path;
    }
    const std::wstring directory = ExeDirectory();
    if (!directory.empty() && path.size() >= directory.size() &&
        ::CompareStringOrdinal(path.c_str(),
                               static_cast<int>(directory.size()),
                               directory.c_str(),
                               static_cast<int>(directory.size()), TRUE) ==
            CSTR_EQUAL) {
        const std::wstring rest = path.substr(directory.size());
        return rest.empty() ? L".\\" : L".\\" + rest;
    }

    wchar_t profile[MAX_PATH] = {};
    const DWORD profile_length =
        ::GetEnvironmentVariableW(L"USERPROFILE", profile,
                                  static_cast<DWORD>(std::size(profile)));
    if (profile_length > 0 && profile_length < std::size(profile)) {
        std::wstring base(profile, profile_length);
        if (!base.empty() && base.back() != L'\\' && base.back() != L'/') {
            base.push_back(L'\\');
        }
        if (path.size() >= base.size() &&
            ::CompareStringOrdinal(path.c_str(), static_cast<int>(base.size()),
                                   base.c_str(), static_cast<int>(base.size()),
                                   TRUE) == CSTR_EQUAL) {
            return L"%USERPROFILE%\\" + path.substr(base.size());
        }
    }
    return path;
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

void ApplyTheme(Microsoft::UI::Xaml::Controls::Grid const& root,
                std::string const& theme) {
    if (theme == "dark") {
        root.RequestedTheme(Microsoft::UI::Xaml::ElementTheme::Dark);
    } else if (theme == "light") {
        root.RequestedTheme(Microsoft::UI::Xaml::ElementTheme::Light);
    } else {
        root.RequestedTheme(Microsoft::UI::Xaml::ElementTheme::Default);
    }
}

const wchar_t* const kLanguageNames[3] = {L"简体中文", L"繁體中文", L"English"};
const char* const kLanguageValues[3] = {"zh_cn", "zh_hk", "en_us"};

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

    AppWindow().Resize({1080, 760});
    try {
        auto area = winrt::Microsoft::UI::Windowing::DisplayArea::GetFromWindowId(
            AppWindow().Id(), winrt::Microsoft::UI::Windowing::DisplayAreaFallback::Primary);
        auto work = area.WorkArea();
        AppWindow().Move({work.X + (work.Width - 1080) / 2,
                          work.Y + (work.Height - 760) / 2});
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
    GeneralStartupTitle().Text(H("resFluentSettingsStartup"));
    GeneralAdvancedTitle().Text(H("resFluentSettingsAdvanced"));

    SetToggleLabel(GeneralRegexLabel(), "ConfigForm.ConfigList_2");
    SetToggleLabel(GeneralMatchAnywhereLabel(), "ConfigForm.ConfigList_3");
    SetToggleLabel(GeneralNumberKeyLabel(), "ConfigForm.ConfigList_4");
    SetToggleLabel(GeneralIndexFrom0Label(), "ConfigForm.ConfigList_5");
    SetToggleLabel(GeneralRememberFavouriteLabel(), "ConfigForm.ConfigList_6");
    SetToggleLabel(GeneralShowTopTenLabel(), "ConfigForm.ConfigList_10");
    SetToggleLabel(GeneralExecuteIfOnlyOneLabel(), "ConfigForm.ConfigList_19");
    SetToggleLabel(GeneralExitWhenExecuteLabel(), "ConfigForm.ConfigList_12");
    SetToggleLabel(GeneralShowCommandLineLabel(), "ConfigForm.ConfigList_8");
    SetToggleLabel(GeneralShowOperationHintLabel(), "ConfigForm.ConfigList_7");
    SetToggleLabel(GeneralAutoRunLabel(), "ConfigForm.ConfigList_0");
    SetToggleLabel(GeneralAddToSendToLabel(), "ConfigForm.ConfigList_1");
    SetToggleLabel(GeneralShowMeWhenStartLabel(), "ConfigForm.ConfigList_14");

    SetNumberHeader(GeneralHideDelay(), "resHideDelay");
    GeneralHideDelayHint().Text(H("resHideDelayHint"));
    SetNumberHeader(GeneralParamHistoryLimit(), "resParamHistoryLimit");
    GeneralParamHistoryHint().Text(H("resParamHistoryLimitHint"));
    GeneralHint().Text(H("resAdvancedHint"));

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
    LanguageLabel().Text(H("ConfigForm.LblLanguage"));
    UpdateLanguageButton();
    SetTextHeader(BackgroundBox(), "ConfigForm.LblBackGroundImage");
    ThemeLabel().Text(H("resTheme"));
    SkinFileLabel().Text(H("resSkinFile"));
    SkinHint().Text(H("resSkinHint"));
    ThemeSystemItem().Text(H("resThemeSystem"));
    ThemeLightItem().Text(H("resThemeLight"));
    ThemeDarkItem().Text(H("resThemeDark"));

    DisplayCardTitle().Text(H("resFluentSettingsDisplay"));
    SetTextHeader(ListFormatBox(), "resListFormat");
    ListFormatHint().Text(H("resListFormatHint"));
    OptionsCardTitle().Text(H("resFluentSettingsOptions"));
    SetToggleLabel(ShowSkinToggleLabel(), "ConfigForm.ConfigList_13");
    SetToggleLabel(ShowTrayIconToggleLabel(), "ConfigForm.ConfigList_15");
    SetToggleLabel(PlayPopupNotifyToggleLabel(), "ConfigForm.ConfigList_11");
    SetToggleLabel(ShowStartNotificationToggleLabel(), "ConfigForm.ConfigList_9");

    AboutCardTitle().Text(H("resAboutCardTitle"));
    AboutTraceLabel().Text(H("resTraceEnable"));
    AboutIniPathLabel().Text(H("resAboutIniPath"));
    AboutParamHistoryLabel().Text(H("resParamHistoryFile"));
    AboutShortcutListLabel().Text(H("resShortcutListFile"));
    AboutFavouriteListLabel().Text(H("resFavoriteListFile"));
    AboutRelatedFilesTitle().Text(H("resRelatedFiles"));
    AboutHint().Text(H("resTraceEnableHint"));

    const hstring open_tip = H("resOpenPath");
    ToolTipService::SetToolTip(AboutIniPathButton(), box_value(open_tip));
    ToolTipService::SetToolTip(AboutIniFolderButton(), box_value(open_tip));
    ToolTipService::SetToolTip(AboutParamHistoryButton(), box_value(open_tip));
    ToolTipService::SetToolTip(AboutParamHistoryFolderButton(), box_value(open_tip));
    ToolTipService::SetToolTip(AboutShortcutListButton(), box_value(open_tip));
    ToolTipService::SetToolTip(AboutShortcutListFolderButton(), box_value(open_tip));
    ToolTipService::SetToolTip(AboutFavouriteListButton(), box_value(open_tip));
    ToolTipService::SetToolTip(AboutFavouriteListFolderButton(), box_value(open_tip));

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
    theme_value_ = settings_.theme;
    if (theme_value_ != "light" && theme_value_ != "dark" && theme_value_ != "system") {
        theme_value_ = "system";
    }
    const char* theme_key = theme_value_ == "dark" ? "resThemeDark" :
                            (theme_value_ == "light" ? "resThemeLight" : "resThemeSystem");
    ThemeButtonText().Text(H(theme_key));
    ApplyTheme(Root(), theme_value_);
    ListFormatBox().Text(hstring(alt::ToWide(settings_.list_format)));

    ShowSkinToggle().IsOn(settings_.show_skin);
    ShowTrayIconToggle().IsOn(settings_.show_tray_icon);
    PlayPopupNotifyToggle().IsOn(settings_.play_popup_notify);
    ShowStartNotificationToggle().IsOn(settings_.show_start_notification);

    int language_index = 2;
    const std::string normalized =
        alt::Localization::NormalizeLanguage(settings_.lang);
    if (normalized == "zh_cn") {
        language_index = 0;
    } else if (normalized == "zh_hk") {
        language_index = 1;
    }
    language_index_ = language_index;
    language_value_ = normalized;
    UpdateLanguageButton();

    AboutVersion().Text(hstring(std::wstring(L"2.1.0")));
    const std::wstring ini_path = settings_.ini_path.empty()
                                     ? ExeDirectory() + L"ALTRun.ini"
                                     : settings_.ini_path;
    AboutIniPath().Text(hstring(DisplayPath(ini_path)));
    ToolTipService::SetToolTip(AboutIniPath(), box_value(hstring(ini_path)));
    AboutIniPathButton().Tag(box_value(hstring(ini_path)));
    AboutIniFolderButton().Tag(box_value(hstring(DirectoryOf(ini_path))));
    AboutRunCount().Text(hstring(alt::LocArgInt(
        W("resAboutRunCount"), settings_.shortcut_run_count)));

    const std::wstring dir = settings_.ini_path.empty()
                                 ? ExeDirectory()
                                 : DirectoryOf(settings_.ini_path);
    const std::wstring param_history =
        (dir.empty() ? ExeDirectory() : dir) + L"ParamHistory.txt";
    const std::wstring shortcut_list =
        (dir.empty() ? ExeDirectory() : dir) + L"ShortCutList.txt";
    const std::wstring favourite_list =
        (dir.empty() ? ExeDirectory() : dir) + L"FavoriteList.txt";

    AboutParamHistory().Text(hstring(DisplayPath(param_history)));
    ToolTipService::SetToolTip(AboutParamHistory(),
                               box_value(hstring(param_history)));
    AboutParamHistoryButton().Tag(box_value(hstring(param_history)));
    AboutParamHistoryFolderButton().Tag(box_value(hstring(DirectoryOf(param_history))));

    AboutShortcutList().Text(hstring(DisplayPath(shortcut_list)));
    ToolTipService::SetToolTip(AboutShortcutList(),
                               box_value(hstring(shortcut_list)));
    AboutShortcutListButton().Tag(box_value(hstring(shortcut_list)));
    AboutShortcutListFolderButton().Tag(box_value(hstring(DirectoryOf(shortcut_list))));

    AboutFavouriteList().Text(hstring(DisplayPath(favourite_list)));
    ToolTipService::SetToolTip(AboutFavouriteList(),
                               box_value(hstring(favourite_list)));
    AboutFavouriteListButton().Tag(box_value(hstring(favourite_list)));
    AboutFavouriteListFolderButton().Tag(box_value(hstring(DirectoryOf(favourite_list))));

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
    settings_.theme = theme_value_;
    settings_.list_format = alt::TrimAscii(U8(ListFormatBox().Text()));

    settings_.show_skin = ShowSkinToggle().IsOn();
    settings_.show_tray_icon = ShowTrayIconToggle().IsOn();
    settings_.play_popup_notify = PlayPopupNotifyToggle().IsOn();
    settings_.show_start_notification = ShowStartNotificationToggle().IsOn();

    if (!language_value_.empty()) {
        settings_.lang = alt::Localization::NormalizeLanguage(language_value_);
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

void MainWindow::UpdateLanguageButton() {
    if (!LanguageButtonText()) {
        return;
    }
    const int index = std::clamp(language_index_, 0, 2);
    LanguageButtonText().Text(hstring(kLanguageNames[index]));
}

void MainWindow::OnLanguageItemClick(
    IInspectable const& sender, RoutedEventArgs const&) {
    auto item = sender.try_as<MenuFlyoutItem>();
    if (!item) {
        return;
    }
    auto tag = item.Tag();
    if (!tag) {
        return;
    }
    const std::string value =
        winrt::to_string(winrt::unbox_value<hstring>(tag));
    for (int index = 0; index < 3; ++index) {
        if (value == kLanguageValues[index]) {
            language_index_ = index;
            language_value_ = value;
            break;
        }
    }
}

void MainWindow::OnThemeItemClick(
    IInspectable const& sender, RoutedEventArgs const&) {
    auto item = sender.try_as<MenuFlyoutItem>();
    if (!item || !item.Tag()) {
        return;
    }
    theme_value_ = winrt::to_string(winrt::unbox_value<hstring>(item.Tag()));
    const char* theme_key = theme_value_ == "dark" ? "resThemeDark" :
                            (theme_value_ == "light" ? "resThemeLight" : "resThemeSystem");
    ThemeButtonText().Text(H(theme_key));
    ApplyTheme(Root(), theme_value_);
}

void MainWindow::OnPickSkinClick(IInspectable const&, RoutedEventArgs const&) {
    wchar_t path[MAX_PATH * 4] = {};
    OPENFILENAMEW dialog = {};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = reinterpret_cast<HWND>(AppWindow().Id().Value);
    dialog.lpstrFilter = L"Images\0*.jpg;*.jpeg;*.png;*.bmp\0All files\0*.*\0";
    dialog.lpstrFile = path;
    dialog.nMaxFile = static_cast<DWORD>(std::size(path));
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    if (::GetOpenFileNameW(&dialog) == FALSE) {
        return;
    }
    const std::wstring selected(path);
    const std::wstring directory = ExeDirectory();
    if (selected.size() >= directory.size() &&
        ::CompareStringOrdinal(selected.c_str(), static_cast<int>(directory.size()),
                               directory.c_str(), static_cast<int>(directory.size()),
                               TRUE) == CSTR_EQUAL) {
        BackgroundBox().Text(hstring(selected.substr(directory.size())));
    } else {
        BackgroundBox().Text(hstring(selected));
    }
}

void MainWindow::OnOpenPathClick(
    IInspectable const& sender, RoutedEventArgs const&) {
    auto button = sender.try_as<Button>();
    if (!button) {
        return;
    }
    auto tag = button.Tag();
    if (!tag) {
        return;
    }
    std::wstring target = std::wstring(winrt::unbox_value<hstring>(tag));
    if (::GetFileAttributesW(target.c_str()) == INVALID_FILE_ATTRIBUTES) {
        target = DirectoryOf(target);
    }
    if (target.empty()) {
        target = ExeDirectory();
    }
    ::ShellExecuteW(nullptr, L"open", target.c_str(), nullptr, nullptr,
                    SW_SHOWNORMAL);
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



