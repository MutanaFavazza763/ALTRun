#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace alt {

// One entry of ParamHistory.txt: usage count + parameter text.
struct ParamHistoryItem {
    int rank = 0;
    std::string text;
};

// Global parameter history shared by every parameterised shortcut
// (original frmParam.pas behaviour):
//   * eviction removes the item with the smallest rank when full
//   * a brand new item is inserted at the front with rank 0
//   * an existing item gets rank + 1 (it is NOT moved to the front)
//
// File: <exe目录>\ParamHistory.txt, ANSI/GBK, one "rank|text" per line.
// Writes plain "rank|text". The legacy writer's trailing '%' behaviour is
// uncertain; preserve literal percent signs rather than guessing at repairs.
class ParamHistory {
public:
    ParamHistory();
    // Explicit path for isolated tests and profile import.
    explicit ParamHistory(std::wstring path);

    bool Load(int limit);
    bool Save(int limit) const;

    const std::vector<ParamHistoryItem>& items() const { return items_; }

    // Records |text| after a successful confirmation.
    void Record(std::string_view text, int limit);

    const std::wstring& path() const { return path_; }

private:
    std::vector<ParamHistoryItem> items_;
    std::wstring path_;
};

}  // namespace alt
