#include "core/Encoding.h"

#include <windows.h>

namespace alt {

std::string AnsiToUtf8(std::string_view ansi, unsigned codepage) {
    if (ansi.empty()) {
        return {};
    }
    const int needed = ::MultiByteToWideChar(
        codepage, 0, ansi.data(), static_cast<int>(ansi.size()), nullptr, 0);
    if (needed <= 0) {
        return {};
    }
    std::wstring wide(static_cast<size_t>(needed), L'\0');
    ::MultiByteToWideChar(codepage, 0, ansi.data(), static_cast<int>(ansi.size()),
                          wide.data(), needed);

    const int utf8_size = ::WideCharToMultiByte(CP_UTF8, 0, wide.data(),
                                                static_cast<int>(wide.size()),
                                                nullptr, 0, nullptr, nullptr);
    if (utf8_size <= 0) {
        return {};
    }
    std::string utf8(static_cast<size_t>(utf8_size), '\0');
    ::WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()),
                          utf8.data(), utf8_size, nullptr, nullptr);
    return utf8;
}

std::string Utf8ToAnsi(std::string_view utf8, unsigned codepage) {
    if (utf8.empty()) {
        return {};
    }
    const int needed = ::MultiByteToWideChar(
        CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
    if (needed <= 0) {
        return {};
    }
    std::wstring wide(static_cast<size_t>(needed), L'\0');
    ::MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()),
                          wide.data(), needed);

    const int ansi_size = ::WideCharToMultiByte(codepage, 0, wide.data(),
                                                static_cast<int>(wide.size()),
                                                nullptr, 0, nullptr, nullptr);
    if (ansi_size <= 0) {
        return {};
    }
    std::string ansi(static_cast<size_t>(ansi_size), '\0');
    ::WideCharToMultiByte(codepage, 0, wide.data(), static_cast<int>(wide.size()),
                          ansi.data(), ansi_size, nullptr, nullptr);
    return ansi;
}

bool IsValidUtf8(std::string_view text) {
    size_t i = 0;
    const size_t size = text.size();
    while (i < size) {
        const auto c = static_cast<unsigned char>(text[i]);
        if (c < 0x80) {
            ++i;
            continue;
        }
        size_t extra = 0;
        unsigned codepoint = 0;
        if ((c & 0xE0) == 0xC0) {
            extra = 1;
            codepoint = c & 0x1F;
        } else if ((c & 0xF0) == 0xE0) {
            extra = 2;
            codepoint = c & 0x0F;
        } else if ((c & 0xF8) == 0xF0) {
            extra = 3;
            codepoint = c & 0x07;
        } else {
            return false;
        }
        if (i + extra >= size + 0 && i + extra > size - 1) {
            return false;
        }
        for (size_t k = 1; k <= extra; ++k) {
            const auto cc = static_cast<unsigned char>(text[i + k]);
            if ((cc & 0xC0) != 0x80) {
                return false;
            }
            codepoint = (codepoint << 6) | (cc & 0x3F);
        }
        // Reject overlong encodings and surrogates.
        if ((extra == 1 && codepoint < 0x80) || (extra == 2 && codepoint < 0x800) ||
            (extra == 3 && codepoint < 0x10000) ||
            (codepoint >= 0xD800 && codepoint <= 0xDFFF) || codepoint > 0x10FFFF) {
            return false;
        }
        i += extra + 1;
    }
    return true;
}

}  // namespace alt
