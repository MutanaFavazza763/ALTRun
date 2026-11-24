#include "core/FontSpec.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstddef>
#include <string_view>
#include <system_error>
#include <vector>

#include "core/Text.h"

namespace alt {
namespace {

constexpr float kDefaultSize = 12.0f;
constexpr unsigned kDefaultColor = 0xFFFFFFu;
constexpr int kDefaultCharset = 0;
constexpr float kMaxSize = 200.0f;

// Splits on '|' keeping empty segments, exactly like the original SplitString
// (untUtilities.pas:261-278) so "SimSun||12||" yields five segments.
std::vector<std::string_view> Split(std::string_view text) {
    std::vector<std::string_view> parts;
    size_t start = 0;
    while (true) {
        const size_t pipe = text.find('|', start);
        if (pipe == std::string_view::npos) {
            parts.push_back(text.substr(start));
            break;
        }
        parts.push_back(text.substr(start, pipe - start));
        start = pipe + 1;
    }
    return parts;
}

std::string_view Segment(const std::vector<std::string_view>& parts, size_t index) {
    return index < parts.size() ? parts[index] : std::string_view();
}

bool ParseFloat(std::string_view text, float* value) {
    if (text.empty() || value == nullptr) {
        return false;
    }
    const char* const begin = text.data();
    const char* const end = begin + text.size();
    const auto result = std::from_chars(begin, end, *value);
    return result.ec == std::errc() && result.ptr == end;
}

bool ParseInt(std::string_view text, int* value) {
    if (text.empty() || value == nullptr) {
        return false;
    }
    const char* const begin = text.data();
    const char* const end = begin + text.size();
    const auto result = std::from_chars(begin, end, *value);
    return result.ec == std::errc() && result.ptr == end;
}

bool ParseUnsigned(std::string_view text, unsigned* value) {
    if (text.empty() || value == nullptr) {
        return false;
    }
    int base = 10;
    if (text.size() > 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) {
        text.remove_prefix(2);
        base = 16;
    }
    const char* const begin = text.data();
    const char* const end = begin + text.size();
    const auto result = std::from_chars(begin, end, *value, base);
    return result.ec == std::errc() && result.ptr == end;
}

// Segment 2 is a Delphi set literal ("[]", "[fsBold]", "[fsBold,fsItalic]").
// The rewrite only needs the bold bit; a plain number is accepted as well.
int ParseStyle(std::string_view text) {
    const std::string_view trimmed = TrimAscii(text);
    if (trimmed.empty() || trimmed == "[]") {
        return 0;
    }
    std::string lowered(trimmed);
    std::transform(lowered.begin(), lowered.end(), lowered.begin(),
                   [](char c) { return static_cast<char>(std::tolower(
                                    static_cast<unsigned char>(c))); });
    if (lowered.find("fsbold") != std::string::npos) {
        return 1;
    }
    int numeric = 0;
    if (ParseInt(trimmed, &numeric)) {
        return numeric != 0 ? 1 : 0;
    }
    return 0;
}

}  // namespace

FontSpec FontSpec::Parse(const std::string& utf8) {
    FontSpec spec;
    const std::vector<std::string_view> parts = Split(utf8);

    const std::string_view family = TrimAscii(Segment(parts, 0));
    if (!family.empty()) {
        spec.family = ToWide(family);
    }

    spec.style = ParseStyle(Segment(parts, 1));

    float size = 0.0f;
    if (ParseFloat(TrimAscii(Segment(parts, 2)), &size) && size > 0.0f &&
        size <= kMaxSize) {
        spec.size = size;
    } else {
        spec.size = kDefaultSize;
    }

    unsigned color = 0;
    if (ParseUnsigned(TrimAscii(Segment(parts, 3)), &color) && color <= 0xFFFFFFu) {
        spec.color = color;
    } else {
        spec.color = kDefaultColor;
    }

    int charset = 0;
    if (ParseInt(TrimAscii(Segment(parts, 4)), &charset) && charset >= 0 &&
        charset <= 0xFF) {
        spec.charset = charset;
    } else {
        spec.charset = kDefaultCharset;
    }
    return spec;
}

std::string FontSpec::Serialize() const {
    std::string out = ToUtf8(family.empty() ? std::wstring_view(L"Segoe UI")
                                            : std::wstring_view(family));
    out.push_back('|');
    out += bold() ? "[fsBold]" : "[]";
    out.push_back('|');

    // Shortest round-trippable decimal, locale independent (no snprintf/locale).
    char size_buffer[32] = {};
    const auto size_result = std::to_chars(size_buffer, size_buffer + sizeof(size_buffer),
                                           size);
    if (size_result.ec == std::errc()) {
        out.append(size_buffer,
                   static_cast<size_t>(size_result.ptr - size_buffer));
    } else {
        out += "12";
    }
    out.push_back('|');
    out += std::to_string(color & 0xFFFFFFu);
    out.push_back('|');
    out += std::to_string(std::clamp(charset, 0, 0xFF));
    return out;
}

}  // namespace alt
