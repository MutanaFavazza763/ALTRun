#include "core/Text.h"

#include <windows.h>

#include <algorithm>
#include <cwctype>

namespace alt {
namespace {

bool IsAsciiSpace(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

}  // namespace

std::wstring ToWide(std::string_view utf8) {
    if (utf8.empty()) {
        return {};
    }
    const int needed = ::MultiByteToWideChar(
        CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
    if (needed <= 0) {
        return {};
    }
    std::wstring out(static_cast<size_t>(needed), L'\0');
    ::MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()),
                          out.data(), needed);
    return out;
}

std::string ToUtf8(std::wstring_view utf16) {
    if (utf16.empty()) {
        return {};
    }
    const int needed = ::WideCharToMultiByte(CP_UTF8, 0, utf16.data(),
                                             static_cast<int>(utf16.size()),
                                             nullptr, 0, nullptr, nullptr);
    if (needed <= 0) {
        return {};
    }
    std::string out(static_cast<size_t>(needed), '\0');
    ::WideCharToMultiByte(CP_UTF8, 0, utf16.data(), static_cast<int>(utf16.size()),
                          out.data(), needed, nullptr, nullptr);
    return out;
}

std::string ToLowerUtf8(std::string_view utf8) {
    const std::wstring wide = ToWide(utf8);
    std::wstring lowered = wide;
    // LCMapStringEx with LCMAP_LOWERCASE handles CJK and full-width forms
    // better than towlower for the launcher's matching semantics.
    const int written = ::LCMapStringEx(
        LOCALE_NAME_INVARIANT, LCMAP_LOWERCASE, wide.c_str(),
        static_cast<int>(wide.size()), lowered.data(),
        static_cast<int>(lowered.size()), nullptr, nullptr, 0);
    if (written <= 0) {
        std::transform(wide.begin(), wide.end(), lowered.begin(),
                       [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
    }
    return ToUtf8(lowered);
}

std::string_view TrimAscii(std::string_view text) {
    size_t begin = 0;
    size_t end = text.size();
    while (begin < end && IsAsciiSpace(text[begin])) {
        ++begin;
    }
    while (end > begin && IsAsciiSpace(text[end - 1])) {
        --end;
    }
    return text.substr(begin, end - begin);
}

bool IEqualsAscii(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) {
        return false;
    }
    for (size_t i = 0; i < a.size(); ++i) {
        char ca = a[i];
        char cb = b[i];
        if (ca >= 'A' && ca <= 'Z') {
            ca = static_cast<char>(ca - 'A' + 'a');
        }
        if (cb >= 'A' && cb <= 'Z') {
            cb = static_cast<char>(cb - 'A' + 'a');
        }
        if (ca != cb) {
            return false;
        }
    }
    return true;
}

}  // namespace alt
