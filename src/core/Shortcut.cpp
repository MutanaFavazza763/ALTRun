#include "core/Shortcut.h"

#include <cctype>

#include "core/Text.h"

namespace alt {
namespace {

bool EqualsFold(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) {
        return false;
    }
    for (size_t i = 0; i < a.size(); ++i) {
        char ca = a[i];
        char cb = b[i];
        if (ca >= 'A' && ca <= 'Z') ca = static_cast<char>(ca - 'A' + 'a');
        if (cb >= 'A' && cb <= 'Z') cb = static_cast<char>(cb - 'A' + 'a');
        if (ca != cb) return false;
    }
    return true;
}

}  // namespace

const char* ParamTypeToStorage(ParamType type) {
    switch (type) {
        case ParamType::None: return "";
        case ParamType::NoEncoding: return "No_Encoding";
        case ParamType::UrlQuery: return "URL_Query";
        case ParamType::Utf8Query: return "UTF8_Query";
    }
    return "";
}

bool ParamTypeTryParse(std::string_view text, ParamType* type) {
    const std::string_view trimmed = TrimAscii(text);
    ParamType result = ParamType::None;
    if (trimmed.empty()) {
        result = ParamType::None;
    } else if (EqualsFold(trimmed, "No_Encoding")) {
        result = ParamType::NoEncoding;
    } else if (EqualsFold(trimmed, "URL_Query")) {
        result = ParamType::UrlQuery;
    } else if (EqualsFold(trimmed, "UTF8_Query")) {
        result = ParamType::Utf8Query;
    } else {
        return false;
    }
    if (type != nullptr) {
        *type = result;
    }
    return true;
}

ParamType ParamTypeFromStorage(std::string_view text) {
    ParamType type = ParamType::None;
    ParamTypeTryParse(text, &type);
    return type;
}

}  // namespace alt
