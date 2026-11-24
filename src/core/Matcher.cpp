#include "core/Matcher.h"

#include <algorithm>
#include <cctype>
#include <regex>

#include "core/Encoding.h"
#include "core/Logger.h"
#include "core/Text.h"

namespace alt {
namespace {

// Delphi's LowerCase only folds ASCII A-Z.
std::string LowerAscii(std::string_view text) {
    std::string out(text);
    for (char& c : out) {
        if (c >= 'A' && c <= 'Z') {
            c = static_cast<char>(c - 'A' + 'a');
        }
    }
    return out;
}

void ReplaceAll(std::string& text, std::string_view from, std::string_view to) {
    if (from.empty()) {
        return;
    }
    size_t pos = 0;
    while ((pos = text.find(from, pos)) != std::string::npos) {
        text.replace(pos, from.size(), to);
        pos += to.size();
    }
}

}  // namespace

std::string Matcher::WildcardToRegex(std::string_view input) {
    std::string pattern(input);
    // Protect existing ".*" so a literal ".*" typed by the user stays intact.
    const std::string sentinel = "\x01\x02\x03";
    ReplaceAll(pattern, ".*", sentinel);
    ReplaceAll(pattern, "*", ".*");
    ReplaceAll(pattern, sentinel, ".*");
    ReplaceAll(pattern, "?", ".");
    return pattern;
}

int Matcher::Score(int freq, int match_pos, size_t keyword_bytes,
                   size_t query_bytes) {
    const int length_delta =
        static_cast<int>(keyword_bytes) - static_cast<int>(query_bytes);
    return 1024 + freq * 4 - match_pos * 128 - length_delta * 16;
}

int Matcher::SortRank(const Shortcut& item) {
    // The original indexes an AnsiString, so ranking is done on GBK bytes.
    const std::string ansi = Utf8ToAnsi(item.keyword);
    int rank = item.freq * 1000;
    int weight = 4;
    for (size_t j = 0; j < 3 && j < ansi.size(); ++j) {
        const auto c = static_cast<unsigned char>(ansi[j]);
        const unsigned char upper = (c >= 'a' && c <= 'z')
                                        ? static_cast<unsigned char>(c - 'a' + 'A')
                                        : c;
        rank -= weight * (static_cast<int>(upper) - static_cast<int>('A'));
        weight /= 2;  // 4, 2, 1
    }
    rank -= static_cast<int>(ansi.size());
    return rank;
}

std::vector<MatchResult> Matcher::Match(const std::vector<ShortcutEntry>& entries,
                                        const MatchQuery& query) const {
    std::vector<MatchResult> results;
    // A single space is a special "no results" value in the original.
    if (query.text == " ") {
        return results;
    }
    if (query.text.empty()) {
        return results;
    }

    const std::string needle = LowerAscii(query.text);

    // Compile the pattern once when in regex mode.
    std::regex pattern;
    bool pattern_ok = false;
    if (query.regex_mode) {
        try {
            pattern = std::regex(WildcardToRegex(needle),
                                 std::regex::ECMAScript | std::regex::optimize);
            pattern_ok = true;
        } catch (const std::regex_error&) {
            Logger::Instance().Warn("invalid regex for query: " + needle);
            pattern_ok = false;
        }
    }

    for (size_t i = 0; i < entries.size(); ++i) {
        const ShortcutEntry& entry = entries[i];
        if (entry.is_blank) {
            continue;
        }
        const Shortcut& item = entry.item;
        if (item.keyword.empty()) {
            continue;
        }

        const std::string haystack = LowerAscii(item.keyword);
        int match_pos = 0;  // 1-based

        if (query.regex_mode) {
            if (!pattern_ok) {
                continue;
            }
            std::smatch match;
            if (!std::regex_search(haystack, match, pattern)) {
                continue;
            }
            match_pos = static_cast<int>(match.position(0)) + 1;
        } else {
            // The original applies this length guard in the non-regex path
            // only (in regex mode it ran after wildcard expansion and wrongly
            // skipped valid matches -- fixed here).
            if (needle.size() > item.keyword.size()) {
                continue;
            }
            const size_t pos = haystack.find(needle);
            if (pos == std::string::npos) {
                continue;
            }
            match_pos = static_cast<int>(pos) + 1;
        }

        if (!query.match_anywhere && match_pos != 1) {
            continue;
        }

        const int score =
            Score(item.freq, match_pos, item.keyword.size(), query.text.size());
        if (score <= 0) {
            continue;
        }

        MatchResult result;
        result.entry_index = i;
        result.score = score;
        result.match_pos = match_pos;
        results.push_back(result);
    }

    // The original used a randomized quicksort (unstable). Deterministic
    // tie-breaking is a deliberate improvement: score desc, then Freq desc,
    // then keyword asc.
    std::stable_sort(results.begin(), results.end(),
                     [&entries](const MatchResult& a, const MatchResult& b) {
                         if (a.score != b.score) {
                             return a.score > b.score;
                         }
                         const Shortcut& left = entries[a.entry_index].item;
                         const Shortcut& right = entries[b.entry_index].item;
                         if (left.freq != right.freq) {
                             return left.freq > right.freq;
                         }
                         return left.keyword < right.keyword;
                     });
    return results;
}

}  // namespace alt
