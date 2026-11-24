#include "core/Formatter.h"

#include <cstdio>

namespace alt {
namespace {

// Returns true when the code point occupies two terminal columns. This is a
// pragmatic East-Asian-width approximation that covers CJK, full-width forms
// and the kana/hangul blocks.
bool IsWideCodepoint(unsigned codepoint) {
    return (codepoint >= 0x1100 && codepoint <= 0x115F) ||
           (codepoint >= 0x2E80 && codepoint <= 0xA4CF) ||
           (codepoint >= 0xAC00 && codepoint <= 0xD7A3) ||
           (codepoint >= 0xF900 && codepoint <= 0xFAFF) ||
           (codepoint >= 0xFE30 && codepoint <= 0xFE6F) ||
           (codepoint >= 0xFF00 && codepoint <= 0xFF60) ||
           (codepoint >= 0xFFE0 && codepoint <= 0xFFE6) ||
           (codepoint >= 0x20000 && codepoint <= 0x3FFFD);
}

size_t Utf8Length(unsigned codepoint) {
    if (codepoint < 0x80) return 1;
    if (codepoint < 0x800) return 2;
    if (codepoint < 0x10000) return 3;
    return 4;
}

unsigned DecodeAt(std::string_view text, size_t index) {
    const auto c = static_cast<unsigned char>(text[index]);
    if (c < 0x80) {
        return c;
    }
    if ((c & 0xE0) == 0xC0 && index + 1 < text.size()) {
        return ((c & 0x1Fu) << 6) |
               (static_cast<unsigned char>(text[index + 1]) & 0x3Fu);
    }
    if ((c & 0xF0) == 0xE0 && index + 2 < text.size()) {
        return ((c & 0x0Fu) << 12) |
               ((static_cast<unsigned char>(text[index + 1]) & 0x3Fu) << 6) |
               (static_cast<unsigned char>(text[index + 2]) & 0x3Fu);
    }
    if ((c & 0xF8) == 0xF0 && index + 3 < text.size()) {
        return ((c & 0x07u) << 18) |
               ((static_cast<unsigned char>(text[index + 1]) & 0x3Fu) << 12) |
               ((static_cast<unsigned char>(text[index + 2]) & 0x3Fu) << 6) |
               (static_cast<unsigned char>(text[index + 3]) & 0x3Fu);
    }
    return c;
}

}  // namespace

size_t DisplayWidth(std::string_view utf8) {
    size_t width = 0;
    size_t i = 0;
    while (i < utf8.size()) {
        const unsigned cp = DecodeAt(utf8, i);
        width += IsWideCodepoint(cp) ? 2 : 1;
        i += Utf8Length(cp) > 0 ? Utf8Length(cp) : 1;
    }
    return width;
}

std::string FormatListLine(std::string_view format, std::string_view keyword,
                           std::string_view name) {
    std::string out;
    size_t arg_index = 0;
    for (size_t i = 0; i < format.size(); ++i) {
        const char c = format[i];
        if (c != '%') {
            out.push_back(c);
            continue;
        }
        if (i + 1 < format.size() && format[i + 1] == '%') {
            out.push_back('%');
            ++i;
            continue;
        }
        // Parse: [-] [width] s
        size_t j = i + 1;
        bool left_align = false;
        if (j < format.size() && format[j] == '-') {
            left_align = true;
            ++j;
        }
        size_t width = 0;
        while (j < format.size() && format[j] >= '0' && format[j] <= '9') {
            width = width * 10 + static_cast<size_t>(format[j] - '0');
            ++j;
        }
        if (j >= format.size() || format[j] != 's') {
            out.push_back(c);  // unsupported conversion: keep literally
            continue;
        }

        std::string_view value = arg_index == 0 ? keyword : name;
        ++arg_index;
        const size_t value_width = DisplayWidth(value);
        if (width <= value_width) {
            out.append(value);
        } else {
            const size_t pad = width - value_width;
            if (left_align) {
                out.append(value);
                out.append(pad, ' ');
            } else {
                out.append(pad, ' ');
                out.append(value);
            }
        }
        i = j;
    }
    return out;
}

ListLineParts FormatListParts(std::string_view format, std::string_view keyword,
                              std::string_view name) {
    ListLineParts parts;
    std::string values[2];
    std::string literals[3];  // before, between, after the two conversions
    size_t conversion_count = 0;
    size_t literal_index = 0;
    bool unsupported = false;

    for (size_t i = 0; i < format.size(); ++i) {
        const char c = format[i];
        if (c != '%') {
            literals[literal_index].push_back(c);
            continue;
        }
        if (i + 1 < format.size() && format[i + 1] == '%') {
            literals[literal_index].push_back('%');
            ++i;
            continue;
        }
        size_t j = i + 1;
        bool left_align = false;
        if (j < format.size() && format[j] == '-') {
            left_align = true;
            ++j;
        }
        size_t width = 0;
        while (j < format.size() && format[j] >= '0' && format[j] <= '9') {
            width = width * 10 + static_cast<size_t>(format[j] - '0');
            ++j;
        }
        if (j >= format.size() || format[j] != 's') {
            unsupported = true;
            break;
        }
        if (conversion_count < 2) {
            std::string_view value = conversion_count == 0 ? keyword : name;
            std::string text(value);
            const size_t value_width = DisplayWidth(value);
            if (width > value_width) {
                const size_t pad = width - value_width;
                if (left_align) {
                    text.append(pad, ' ');
                } else {
                    text.insert(0, pad, ' ');
                }
            }
            values[conversion_count] = std::move(text);
        }
        ++conversion_count;
        if (literal_index < 2) {
            ++literal_index;
        }
        i = j;
    }

    if (unsupported || conversion_count != 2 || literals[1].find('|') == std::string::npos) {
        parts.left = FormatListLine(format, keyword, name);
        return parts;
    }
    parts.two_columns = true;
    parts.left = std::move(values[0]);
    parts.separator = std::move(literals[1]);
    parts.right = std::move(values[1]);
    if (!literals[2].empty()) {
        parts.right += literals[2];
    }
    return parts;
}


std::string NumberPrefix(size_t row, bool has_param, bool index_from_0_to_9) {
    if (row >= 10) {
        return "  |";
    }
    int number = 0;
    if (index_from_0_to_9) {
        number = static_cast<int>(row);
    } else {
        number = row == 9 ? 0 : static_cast<int>(row) + 1;
    }
    std::string prefix;
    prefix.push_back(has_param ? '*' : ' ');
    prefix += std::to_string(number);
    prefix.push_back('|');
    return prefix;
}

}  // namespace alt
