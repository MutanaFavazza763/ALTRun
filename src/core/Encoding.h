#pragma once

// Legacy text encoding helpers.
//
// The original application stored ShortCutList.txt / FavoriteList.txt as
// Delphi `TextFile` (AnsiString), i.e. the system ANSI code page -- GBK/936 on
// Simplified Chinese Windows. The rewrite keeps UTF-8 internally and converts
// only at the file boundary so old profiles stay readable and editable by the
// old binary.

#include <string>
#include <string_view>

namespace alt {

constexpr unsigned kAnsiCodePage = 936;  // GBK / cp936

std::string AnsiToUtf8(std::string_view ansi, unsigned codepage = kAnsiCodePage);
std::string Utf8ToAnsi(std::string_view utf8, unsigned codepage = kAnsiCodePage);

// Strict UTF-8 validation, used to guess the encoding of an existing file.
bool IsValidUtf8(std::string_view text);

}  // namespace alt
