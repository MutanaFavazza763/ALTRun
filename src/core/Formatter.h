#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace alt {

// Result list text formatting, reproducing the original ListFormat option
// (default "%-25s| %s", alternatives "%s (%s)", "%s [%s]").
//
// Only printf-style "s" conversions with an optional '-' flag and width are
// supported, which is exactly the set the original offered. Width is measured
// in display columns (CJK counts as two) so columns line up regardless of the
// font.
std::string FormatListLine(std::string_view format, std::string_view keyword,
                           std::string_view name);

// Number prefix used by the launcher list when NumberKey is enabled:
//   first 10 rows: " 1|" for plain items, "*2|" when the item prompts for a
//   parameter; rows beyond the first 10 show "  |".
// |index_from_0_to_9| selects the 0..9 or 1..9,0 numbering scheme.
std::string NumberPrefix(size_t row, bool has_param, bool index_from_0_to_9);

// Split form used by the launcher list: the text before and after the literal
// separator between the two "%s" conversions ("%-25s| %s" -> keyword, "| ",
// name).  The launcher draws these in fixed pixel columns instead of relying on
// space padding, so proportional UI fonts still line up.
struct ListLineParts {
    std::string left;       // text of the first conversion (keyword)
    std::string separator;  // literal text between the conversions ("| ")
    std::string right;      // text of the second conversion (display name)
    bool two_columns = false;
};

ListLineParts FormatListParts(std::string_view format, std::string_view keyword,
                              std::string_view name);

// Display width helper (CJK wide characters count as 2 columns).
size_t DisplayWidth(std::string_view utf8);

}  // namespace alt
