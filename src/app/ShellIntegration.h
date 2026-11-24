#pragma once

#include <string>

namespace alt {

// Win32 shell integration that lives outside the portable ALTRun.ini:
//
//   * AddToSendTo  -> %APPDATA%\Microsoft\Windows\SendTo\ALTRun.lnk
//   * PlayPopupNotify -> <exe目录>\Popup.wav through PlaySoundW
//
// See docs/spec/04-aux-features.md §6/§7 and docs/spec/02-config-options.md §1.1.
class ShellIntegration {
public:
    // Full path of the running ALTRun.exe and its directory (trailing '\').
    static std::wstring ExecutablePath();
    static std::wstring ExecutableDirectory();

    // Creates or deletes the "SendTo" shortcut.  Idempotent: when enabled the
    // shortcut is always rebuilt so a moved executable is picked up; when
    // disabled the file is removed (a missing file is not an error).
    static bool ApplyAddToSendTo(bool enabled, const std::wstring& exe_path);
    static std::wstring SendToShortcutPath();

    // Plays <exe目录>\Popup.wav asynchronously when enabled.  A missing wav is
    // silently ignored (the original extracted a default one from resources;
    // the rewrite ships no resources).
    static void PlayPopupSound(bool enabled);
};

}  // namespace alt
