#pragma once

#include <string>

namespace alt {

// Application settings.
//
// Portable, exactly like the original: the ini lives next to ALTRun.exe and is
// ANSI-encoded (written through the Win32 profile API, which converts to the
// system code page). Key names match Bin\ALTRun.ini so an existing profile keeps
// working; see docs/spec/02-config-options.md for the authoritative list.
struct Settings {
    // [Config] -- 25 keys, names identical to the legacy ini.
    std::string hotkey1 = "Alt + R";       // HotKey1   main show/hide hotkey
    std::string hotkey2 = "Pause";         // HotKey2   secondary hotkey ("" disables)
    std::string last_item_hotkey = "ALT+L";  // LastItemHotKey
    bool auto_run = true;                 // AutoRun
    bool regex_mode = true;               // Regex
    // HideDelay: seconds until the launcher hides itself while idle
    // (spec confirms the unit is seconds, not ticks).
    int hide_delay = 15;
    bool match_anywhere = true;           // MatchAnywhere
    bool number_key = true;               // NumberKey
    bool index_from_0_to_9 = false;       // IndexFrom0to9
    bool remember_favourite_match = false;  // RememberFavouratMatch
    int param_history_limit = 50;         // ParamHistoryLimit
    bool show_operation_hint = true;      // ShowOperationHint
    bool show_command_line = true;        // ShowCommandLine
    bool show_start_notification = true;  // ShowStartNotification
    bool show_top_ten = true;             // ShowTopTen
    bool play_popup_notify = true;        // PlayPopupNotify
    bool exit_when_execute = false;       // ExitWhenExecute
    bool show_skin = true;                // ShowSkin
    bool show_me_when_start = false;      // ShowMeWhenStart
    bool show_tray_icon = true;           // ShowTrayIcon
    bool show_shortcut_button = false;    // ShowShortCutButton
    bool show_config_button = false;      // ShowConfigButton
    bool show_close_button = false;       // ShowCloseButton
    bool execute_if_only_one = false;     // ExecuteIfOnlyOne
    bool add_to_send_to = true;           // AddToSendTo

    // [Window]
    int win_top = -1;   // -1 = center on the cursor's monitor
    int win_left = -1;

    // [GUI]
    std::string bg_file_name = "BG.jpg";  // BGFileName
    // TitleFont / KeywordFont / ListFont: legacy 5-segment font strings
    // (<family>|[]|12|65535|0); parsed by core/FontSpec.
    std::string title_font = "Segoe UI|[]|12|65535|0";    // TitleFont
    std::string keyword_font = "Segoe UI|[]|12|255|0";    // KeywordFont
    std::string list_font = "Segoe UI|[]|12|8388608|0";   // ListFont
    std::string lang = "en_us";           // Lang: zh_cn / zh_hk / en_us
    std::string list_format = "%-25s| %s";  // ListFormat
    int alpha = 240;                      // Alpha
    int round_border_radius = 12;         // RoundBorderRadius
    int form_width = 420;                 // FormWidth

    // [DEBUG]
    bool trace_enabled = false;           // TraceEnable
    int shortcut_run_count = 0;           // ShortCutRunCount
    // LatestList: comma separated ShortCutList row indices (max 10).
    std::string latest_list;              // LatestList

    // Resolved at runtime, not persisted.
    std::wstring ini_path;
    // True when Load() found no ALTRun.ini (portable or roaming).  App shows
    // the first-run welcome window once and Save() clears the flag by creating
    // the ini.
    bool first_run = false;

    // Loads <exe目录>\ALTRun.ini (falls back to %APPDATA%\ALTRun\ALTRun.ini
    // only when the portable file does not exist, then to built-in defaults).
    void Load();
    bool Save() const;
};

}  // namespace alt
