#pragma once

#include <string>
#include <string_view>

namespace alt {

// Parameter handling mode, mirroring the original TParamType
// (untShortCutMan.pas:33). Persisted as the 2nd field of a ShortCutList.txt
// line; an empty field means ParamType::None.
enum class ParamType {
    None,        // ""            no parameter prompt
    NoEncoding,  // "No_Encoding" parameter used verbatim
    UrlQuery,    // "URL_Query"   parameter ANSI percent-encoded
    Utf8Query,   // "UTF8_Query"  parameter UTF-8 percent-encoded
};

// The exact string written to / read from ShortCutList.txt.
const char* ParamTypeToStorage(ParamType type);

// Case-insensitive; empty text maps to ParamType::None and returns true.
// An unknown non-empty string returns false without touching |type|
// (the original parser then treats the field as the keyword).
bool ParamTypeTryParse(std::string_view text, ParamType* type);

// Convenience wrapper: unknown text also yields ParamType::None.
ParamType ParamTypeFromStorage(std::string_view text);

// One shortcut item. This is deliberately the *flat* original model: there is
// no separate target/args/working-dir/icon/remark/favourite field, because the
// legacy format does not have them (see docs/spec/03-shortcut-model.md §2.2).
struct Shortcut {
    ParamType param_type = ParamType::None;
    // ShortCut: the only string matched against user input.
    std::string keyword;
    // Name: display name only, never matched.
    std::string name;
    // CommandLine: executable/document/folder/URL plus optional arguments and
    // %p / {%p} placeholders. Target and working directory are derived at
    // execution time (Launcher.cpp).
    std::string command_line;
    // Freq: number of times launched from a search result.
    int freq = 0;

    // UI label: Name when present, otherwise the keyword.
    std::string DisplayName() const { return name.empty() ? keyword : name; }

    // True when executing this item asks the user for a parameter.
    bool HasParam() const { return param_type != ParamType::None; }
};

// A physical row of ShortCutList.txt. Blank rows are preserved because the
// original uses them as visual separators inside the manager list
// (TShortCutType.scBlank).
struct ShortcutEntry {
    bool is_blank = false;
    Shortcut item;
};

}  // namespace alt
