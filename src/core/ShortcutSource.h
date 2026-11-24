#pragma once

#include <string>

#include "core/Shortcut.h"

namespace alt {

// Derivation of Shortcut fields from a path/URL/command line, mirroring the
// legacy ExtractShortCutItemFromFileName / ResolveLink helpers
// (docs/spec/03-shortcut-model.md §8.1):
//
//   * plain file:  keyword = file name without directory and extension,
//                  name    = keyword, command_line = the original path
//   * directory:   keyword = full directory name (no extension stripping)
//   * *.lnk:       IShellLink resolution; command_line = "target" + arguments;
//                  when resolution fails (or points at an installer icon stub)
//                  the .lnk path itself is kept as command_line
//   * http/https:  command_line = the URL, keyword = host name (text after
//                  "://" up to the first '/'), name = keyword
//
// Environment variables inside the input are never expanded (%VAR% stays).
// Extension checks are case insensitive.

// Derives the fields from a file/directory path or a URL. Everything except
// command_line/keyword/name is left at its default (ParamType::None, Freq 0).
Shortcut ShortcutFromPath(const std::string& path_utf8);

// Derives the fields from an http/https URL. A non-URL input is still stored
// verbatim as command_line (keyword derivation just yields an empty keyword).
Shortcut ShortcutFromUrl(const std::string& url_utf8);

// Derives the fields from a command line such as `"C:\x y\a.exe" -arg`,
// `chrome.exe --app=https://example.com` or a quoted path. The original full
// command line is always preserved in command_line.
Shortcut ShortcutFromCommandLine(const std::string& command_line_utf8);

// Resolves a .lnk file through IShellLinkW + IPersistFile and returns
// "\"<target>\" <arguments>" (arguments omitted when empty). Returns an empty
// string when the file cannot be resolved; a missing CoInitializeEx is not
// fatal (the failure is reported as an empty result).
std::string ResolveLnk(const std::wstring& lnk_path);

// Converts between absolute paths and paths relative to the executable
// directory (ShortcutStore::ExeDirectory()), like the original manager's path
// conversion command. ToRelativePath leaves paths outside the executable
// directory untouched; ToAbsolutePath only rewrites ".", ".\..." and "..\..."
// style input. Environment variables are not expanded.
std::string ToRelativePath(const std::string& absolute_utf8);
std::string ToAbsolutePath(const std::string& path_utf8);

}  // namespace alt
