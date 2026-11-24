#pragma once

// Shared UI text lookup.
//
// Every window renders through Localization so one <Lang>.lang file (or the
// built-in 224-key table) switches the whole interface.  Keys are either
// qualified legacy names ("ConfigForm.ConfigList_0") or rewrite-only res*
// keys; see core/Localization.h for the lookup order.

#include <string>

#include "core/Localization.h"

namespace alt {

inline std::wstring Loc(const std::string& key) {
    return Localization::Instance().Get(key);
}

// printf-style widening for the legacy patterns that carry one "%s"/"%d".
// (The original built these with Format/FormatStr; keeping the same %s shape
// means a .lang file can reorder or reword it freely.)
inline std::wstring LocArg(std::wstring pattern, const std::wstring& value) {
    const size_t at = pattern.find(L"%s");
    if (at != std::wstring::npos) {
        pattern.replace(at, 2, value);
    }
    return pattern;
}

inline std::wstring LocArgInt(std::wstring pattern, int value) {
    const size_t at = pattern.find(L"%d");
    if (at != std::wstring::npos) {
        pattern.replace(at, 2, std::to_wstring(value));
    }
    return pattern;
}

}  // namespace alt
