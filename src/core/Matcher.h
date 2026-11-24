#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "core/Shortcut.h"

namespace alt {

struct MatchQuery {
    // Raw text typed by the user (not trimmed, like the original edit box).
    std::string text;
    bool regex_mode = true;      // [Config] Regex
    bool match_anywhere = true;  // [Config] MatchAnywhere
};

struct MatchResult {
    // Index into ShortcutStore::entries() (blank rows never match).
    size_t entry_index = 0;
    int score = 0;
    int match_pos = 0;  // 1-based position of the hit
};

// Reproduces the original filtering engine
// (untShortCutMan.pas FilterKeyWord, docs/spec/03-shortcut-model.md §5):
//   * only the ShortCut keyword is matched, never Name or CommandLine
//   * case folding is ASCII-only
//   * regex mode converts '*' -> '.*' and '?' -> '.'
//   * score = 1024 + Freq*4 - match_pos*128 - (len(keyword)-len(query))*16
class Matcher {
public:
    std::vector<MatchResult> Match(const std::vector<ShortcutEntry>& entries,
                                   const MatchQuery& query) const;

    // Sort() rank used when the query is empty:
    //   Rank = Freq*1000 - 4*(S[1]-'A') - 2*(S[2]-'A') - 1*(S[3]-'A') - Length(S)
    // computed over the ANSI/GBK bytes, exactly like the Delphi original.
    static int SortRank(const Shortcut& item);

    // '*' -> '.*', '?' -> '.', leaving existing ".*" intact.
    static std::string WildcardToRegex(std::string_view input);

    // Exposed for tests: the ranking formula in isolation.
    static int Score(int freq, int match_pos, size_t keyword_bytes,
                     size_t query_bytes);
};

}  // namespace alt
