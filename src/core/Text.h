#pragma once

// UTF-8 <-> UTF-16 conversion helpers.
// The original Delphi code stored everything as ANSI (GBK on Chinese systems)
// inside an INI file. We keep INI compatibility for reading/writing legacy
// files but work with UTF-8 strings internally and UTF-16 at the Win32 border.

#include <string>
#include <string_view>

namespace alt {

std::wstring ToWide(std::string_view utf8);
std::string ToUtf8(std::wstring_view utf16);

// Case folding that is safe for both ASCII and CJK input.
std::string ToLowerUtf8(std::string_view utf8);

// Trims ASCII whitespace (space, tab, CR, LF) from both ends.
std::string_view TrimAscii(std::string_view text);

bool IEqualsAscii(std::string_view a, std::string_view b);

}  // namespace alt
