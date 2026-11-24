#pragma once

#include <string>
#include <string_view>

#include "core/Shortcut.h"

namespace alt {

// Everything needed to start a process, derived from a Shortcut's CommandLine.
struct LaunchPlan {
    std::wstring command;        // lpFile for ShellExecuteEx
    std::wstring parameters;     // lpParameters (may be empty)
    std::wstring working_dir;    // lpDirectory (may be empty)
    std::wstring raw_command_line;  // used by the WinExec fallback
    int show_command = 1;        // SW_SHOWNORMAL
};

struct LaunchResult {
    bool ok = false;
    unsigned long error = 0;
    std::string message;
};

// Builds the plan for |item|. |param| is the user supplied parameter (already
// unencoded); it is encoded according to the item's ParamType and substituted
// into the first placeholder found, or appended to the command line when the
// command has no placeholder (original behaviour, see spec §8.5).
LaunchPlan BuildLaunchPlan(const Shortcut& item, std::string_view param);

// Executes a plan: ShellExecuteEx first, WinExec as the original's fallback.
LaunchResult ExecutePlan(const LaunchPlan& plan);

// Build + execute convenience wrapper.
LaunchResult LaunchShortcut(const Shortcut& item, std::string_view param);

// '%' percent encoding over the given bytes (" " -> "%20"). Used for
// URL_Query (ANSI bytes) and UTF8_Query (UTF-8 bytes).
std::string PercentEncode(std::string_view bytes);

// Encodes |param| according to |type|.
std::string EncodeParam(ParamType type, std::string_view param);

// Text currently on the clipboard (empty when unavailable), used by {%c}.
std::string ClipboardText();

}  // namespace alt
