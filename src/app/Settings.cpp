#include "app/Settings.h"

#include <windows.h>

#include <string>

#include "core/Encoding.h"
#include "core/Localization.h"
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

// Windows' profile API reads ALTRun.ini as ANSI, or as UTF-16 when the file
// carries a byte order mark. Nothing in between. Notepad on Windows 11 saves
// UTF-8 by default, and then "Lang=简体中文" is read back as "绠€浣撲腑鏂", so the
// launcher silently falls back to the built-in English strings (and the same
// corruption would hit any non-ASCII font name or list format).
//
// Convert such a file to UTF-16 once, in place: every later read/write through
// the profile API keeps the text intact. Legacy ANSI/GBK files are left
// untouched, so old profiles stay byte compatible with the original binary.
void NormalizeIniEncoding(const std::wstring& ini) {
    HANDLE file = ::CreateFileW(ini.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                                OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return;  // first run: nothing to normalise yet
    }
    LARGE_INTEGER file_size = {};
    if (::GetFileSizeEx(file, &file_size) == FALSE || file_size.QuadPart <= 0 ||
        file_size.QuadPart > 4 * 1024 * 1024) {
        ::CloseHandle(file);
        return;
    }

    std::string bytes(static_cast<size_t>(file_size.QuadPart), '\0');
    DWORD read = 0;
    const bool read_ok =
        ::ReadFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &read,
                   nullptr) != FALSE;
    ::CloseHandle(file);
    if (!read_ok) {
        return;
    }
    bytes.resize(read);

    // Already UTF-16 (either endianness): the profile API reads it as is.
    if (bytes.size() >= 2 &&
        ((bytes[0] == '\xFF' && bytes[1] == '\xFE') ||
         (bytes[0] == '\xFE' && bytes[1] == '\xFF'))) {
        return;
    }

    const bool has_bom =
        bytes.size() >= 3 && bytes[0] == '\xEF' && bytes[1] == '\xBB' && bytes[2] == '\xBF';
    const std::string body = has_bom ? bytes.substr(3) : bytes;

    bool ascii_only = true;
    for (const unsigned char c : body) {
        if (c >= 0x80) {
            ascii_only = false;
            break;
        }
    }
    if (ascii_only) {
        return;  // plain ASCII reads identically either way
    }
    // A legacy GBK file is not valid UTF-8, so it stays ANSI.
    if (!IsValidUtf8(body)) {
        return;
    }

    const std::wstring wide = ToWide(body);
    std::string utf16;
    utf16.reserve(2 + wide.size() * 2);
    utf16.push_back('\xFF');
    utf16.push_back('\xFE');
    for (const wchar_t ch : wide) {
        utf16.push_back(static_cast<char>(ch & 0xFF));
        utf16.push_back(static_cast<char>((ch >> 8) & 0xFF));
    }

    const std::wstring temp = ini + L".u8.tmp";
    HANDLE out = ::CreateFileW(temp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                               FILE_ATTRIBUTE_NORMAL, nullptr);
    if (out == INVALID_HANDLE_VALUE) {
        return;
    }
    DWORD written = 0;
    const bool write_ok =
        ::WriteFile(out, utf16.data(), static_cast<DWORD>(utf16.size()), &written,
                    nullptr) != FALSE;
    ::CloseHandle(out);
    if (!write_ok || written != utf16.size() ||
        ::MoveFileExW(temp.c_str(), ini.c_str(), MOVEFILE_REPLACE_EXISTING) == FALSE) {
        ::DeleteFileW(temp.c_str());
        return;
    }
    Logger::Instance().Info("settings: converted UTF-8 ini to UTF-16");
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
    NormalizeIniEncoding(ini);

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
    // Empty values fall back to the current (default) font strings, matching
    // the original "empty -> DEFAULT_*_FONT_STR" rule (spec §1.5).
    title_font = ReadString(ini, L"GUI", L"TitleFont", title_font);
    keyword_font = ReadString(ini, L"GUI", L"KeywordFont", keyword_font);
    list_font = ReadString(ini, L"GUI", L"ListFont", list_font);
    lang = Localization::NormalizeLanguage(ReadString(ini, L"GUI", L"Lang", lang));
    list_format = ReadString(ini, L"GUI", L"ListFormat", list_format);
    alpha = ReadInt(ini, L"GUI", L"Alpha", alpha);
    round_border_radius = ReadInt(ini, L"GUI", L"RoundBorderRadius", round_border_radius);
    form_width = ReadInt(ini, L"GUI", L"FormWidth", form_width);

    trace_enabled = ReadBool(ini, L"DEBUG", L"TraceEnable", false);
    shortcut_run_count = ReadInt(ini, L"DEBUG", L"ShortCutRunCount", 0);
    latest_list = ReadString(ini, L"DEBUG", L"LatestList", latest_list);

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
    WriteStr(ini, L"GUI", L"TitleFont", title_font);
    WriteStr(ini, L"GUI", L"KeywordFont", keyword_font);
    WriteStr(ini, L"GUI", L"ListFont", list_font);
    WriteStr(ini, L"GUI", L"Lang", Localization::NormalizeLanguage(lang));
    WriteStr(ini, L"GUI", L"ListFormat", list_format);
    WriteInt(ini, L"GUI", L"Alpha", alpha);
    WriteInt(ini, L"GUI", L"RoundBorderRadius", round_border_radius);
    WriteInt(ini, L"GUI", L"FormWidth", form_width);

    WriteBool(ini, L"DEBUG", L"TraceEnable", trace_enabled);
    WriteInt(ini, L"DEBUG", L"ShortCutRunCount", shortcut_run_count);
    WriteStr(ini, L"DEBUG", L"LatestList", latest_list);

    return true;
}

}  // namespace alt
