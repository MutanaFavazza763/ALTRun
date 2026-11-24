#include "app/Settings.h"

#include <windows.h>

#include <string>

#include "core/Logger.h"
#include "core/Text.h"

namespace alt {
namespace {

std::wstring ExeDirectory() {
    wchar_t exe[MAX_PATH * 2] = {};
    ::GetModuleFileNameW(nullptr, exe, static_cast<DWORD>(std::size(exe)));
    std::wstring path = exe;
    const size_t slash = path.find_last_of(L"\\/");
    if (slash != std::wstring::npos) {
        path.resize(slash + 1);
    }
    return path;
}

std::wstring AppDataIniPath() {
    wchar_t buffer[MAX_PATH * 2] = {};
    const DWORD written = ::GetEnvironmentVariableW(
        L"APPDATA", buffer, static_cast<DWORD>(std::size(buffer)));
    std::wstring dir;
    if (written > 0 && written < std::size(buffer)) {
        dir.assign(buffer, written);
    }
    if (dir.empty()) {
        return L"";
    }
    return dir + L"\\ALTRun\\ALTRun.ini";
}

std::string ReadString(const std::wstring& ini, const wchar_t* section,
                       const wchar_t* key, const std::string& fallback) {
    wchar_t buffer[4096] = {};
    ::GetPrivateProfileStringW(section, key, L"", buffer,
                               static_cast<DWORD>(std::size(buffer)), ini.c_str());
    if (buffer[0] == L'\0') {
        return fallback;
    }
    return ToUtf8(buffer);
}

int ReadInt(const std::wstring& ini, const wchar_t* section, const wchar_t* key,
            int fallback) {
    return static_cast<int>(
        ::GetPrivateProfileIntW(section, key, fallback, ini.c_str()));
}

bool ReadBool(const std::wstring& ini, const wchar_t* section, const wchar_t* key,
              bool fallback) {
    return ReadInt(ini, section, key, fallback ? 1 : 0) != 0;
}

void WriteInt(const std::wstring& ini, const wchar_t* section, const wchar_t* key,
              int value) {
    ::WritePrivateProfileStringW(section, key, std::to_wstring(value).c_str(),
                                 ini.c_str());
}

void WriteBool(const std::wstring& ini, const wchar_t* section, const wchar_t* key,
               bool value) {
    ::WritePrivateProfileStringW(section, key, value ? L"1" : L"0", ini.c_str());
}

void WriteStr(const std::wstring& ini, const wchar_t* section, const wchar_t* key,
              const std::string& value) {
    ::WritePrivateProfileStringW(section, key, ToWide(value).c_str(), ini.c_str());
}

}  // namespace

void Settings::Load() {
    // Portable layout first: <exe目录>\ALTRun.ini (original behaviour).
    const std::wstring portable = ExeDirectory() + L"ALTRun.ini";
    ini_path = portable;
    if (::GetFileAttributesW(portable.c_str()) == INVALID_FILE_ATTRIBUTES) {
        const std::wstring roaming = AppDataIniPath();
        if (!roaming.empty() &&
            ::GetFileAttributesW(roaming.c_str()) != INVALID_FILE_ATTRIBUTES) {
            ini_path = roaming;
            Logger::Instance().Info("using roaming ALTRun.ini");
        }
    }

    const std::wstring& ini = ini_path;

    hotkey1 = ReadString(ini, L"Config", L"HotKey1", hotkey1);
    hotkey2 = ReadString(ini, L"Config", L"HotKey2", hotkey2);
    last_item_hotkey = ReadString(ini, L"Config", L"LastItemHotKey", last_item_hotkey);
    auto_run = ReadBool(ini, L"Config", L"AutoRun", auto_run);
    add_to_send_to = ReadBool(ini, L"Config", L"AddToSendTo", add_to_send_to);
    regex_mode = ReadBool(ini, L"Config", L"Regex", regex_mode);
    hide_delay = ReadInt(ini, L"Config", L"HideDelay", hide_delay);
    match_anywhere = ReadBool(ini, L"Config", L"MatchAnywhere", match_anywhere);
    number_key = ReadBool(ini, L"Config", L"NumberKey", number_key);
    index_from_0_to_9 = ReadBool(ini, L"Config", L"IndexFrom0to9", index_from_0_to_9);
    remember_favourite_match =
        ReadBool(ini, L"Config", L"RememberFavouratMatch", remember_favourite_match);
    param_history_limit =
        ReadInt(ini, L"Config", L"ParamHistoryLimit", param_history_limit);
    show_operation_hint =
        ReadBool(ini, L"Config", L"ShowOperationHint", show_operation_hint);
    show_command_line = ReadBool(ini, L"Config", L"ShowCommandLine", show_command_line);
    show_start_notification =
        ReadBool(ini, L"Config", L"ShowStartNotification", show_start_notification);
    show_top_ten = ReadBool(ini, L"Config", L"ShowTopTen", show_top_ten);
    play_popup_notify =
        ReadBool(ini, L"Config", L"PlayPopupNotify", play_popup_notify);
    exit_when_execute =
        ReadBool(ini, L"Config", L"ExitWhenExecute", exit_when_execute);
    show_skin = ReadBool(ini, L"Config", L"ShowSkin", show_skin);
    show_me_when_start =
        ReadBool(ini, L"Config", L"ShowMeWhenStart", show_me_when_start);
    show_tray_icon = ReadBool(ini, L"Config", L"ShowTrayIcon", show_tray_icon);
    show_shortcut_button =
        ReadBool(ini, L"Config", L"ShowShortCutButton", show_shortcut_button);
    show_config_button =
        ReadBool(ini, L"Config", L"ShowConfigButton", show_config_button);
    show_close_button = ReadBool(ini, L"Config", L"ShowCloseButton", show_close_button);
    execute_if_only_one =
        ReadBool(ini, L"Config", L"ExecuteIfOnlyOne", execute_if_only_one);

    win_top = ReadInt(ini, L"Window", L"WinTop", win_top);
    win_left = ReadInt(ini, L"Window", L"WinLeft", win_left);

    bg_file_name = ReadString(ini, L"GUI", L"BGFileName", bg_file_name);
    lang = ReadString(ini, L"GUI", L"Lang", lang);
    list_format = ReadString(ini, L"GUI", L"ListFormat", list_format);
    alpha = ReadInt(ini, L"GUI", L"Alpha", alpha);
    round_border_radius = ReadInt(ini, L"GUI", L"RoundBorderRadius", round_border_radius);
    form_width = ReadInt(ini, L"GUI", L"FormWidth", form_width);

    trace_enabled = ReadBool(ini, L"DEBUG", L"TraceEnable", false);
    shortcut_run_count = ReadInt(ini, L"DEBUG", L"ShortCutRunCount", 0);

    Logger::Instance().SetEnabled(trace_enabled);
    Logger::Instance().Info("settings: " + ToUtf8(ini_path));
}

bool Settings::Save() const {
    const std::wstring ini = ini_path.empty() ? ExeDirectory() + L"ALTRun.ini"
                                              : ini_path;

    WriteStr(ini, L"Config", L"HotKey1", hotkey1);
    WriteStr(ini, L"Config", L"HotKey2", hotkey2);
    WriteStr(ini, L"Config", L"LastItemHotKey", last_item_hotkey);
    WriteBool(ini, L"Config", L"AutoRun", auto_run);
    WriteBool(ini, L"Config", L"AddToSendTo", add_to_send_to);
    WriteBool(ini, L"Config", L"Regex", regex_mode);
    WriteInt(ini, L"Config", L"HideDelay", hide_delay);
    WriteBool(ini, L"Config", L"MatchAnywhere", match_anywhere);
    WriteBool(ini, L"Config", L"NumberKey", number_key);
    WriteBool(ini, L"Config", L"IndexFrom0to9", index_from_0_to_9);
    WriteBool(ini, L"Config", L"RememberFavouratMatch", remember_favourite_match);
    WriteInt(ini, L"Config", L"ParamHistoryLimit", param_history_limit);
    WriteBool(ini, L"Config", L"ShowOperationHint", show_operation_hint);
    WriteBool(ini, L"Config", L"ShowCommandLine", show_command_line);
    WriteBool(ini, L"Config", L"ShowStartNotification", show_start_notification);
    WriteBool(ini, L"Config", L"ShowTopTen", show_top_ten);
    WriteBool(ini, L"Config", L"PlayPopupNotify", play_popup_notify);
    WriteBool(ini, L"Config", L"ExitWhenExecute", exit_when_execute);
    WriteBool(ini, L"Config", L"ShowSkin", show_skin);
    WriteBool(ini, L"Config", L"ShowMeWhenStart", show_me_when_start);
    WriteBool(ini, L"Config", L"ShowTrayIcon", show_tray_icon);
    WriteBool(ini, L"Config", L"ShowShortCutButton", show_shortcut_button);
    WriteBool(ini, L"Config", L"ShowConfigButton", show_config_button);
    WriteBool(ini, L"Config", L"ShowCloseButton", show_close_button);
    WriteBool(ini, L"Config", L"ExecuteIfOnlyOne", execute_if_only_one);

    WriteInt(ini, L"Window", L"WinTop", win_top);
    WriteInt(ini, L"Window", L"WinLeft", win_left);

    WriteStr(ini, L"GUI", L"BGFileName", bg_file_name);
    WriteStr(ini, L"GUI", L"Lang", lang);
    WriteStr(ini, L"GUI", L"ListFormat", list_format);
    WriteInt(ini, L"GUI", L"Alpha", alpha);
    WriteInt(ini, L"GUI", L"RoundBorderRadius", round_border_radius);
    WriteInt(ini, L"GUI", L"FormWidth", form_width);

    WriteBool(ini, L"DEBUG", L"TraceEnable", trace_enabled);
    WriteInt(ini, L"DEBUG", L"ShortCutRunCount", shortcut_run_count);

    return true;
}

}  // namespace alt
