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
// The original wrote Format('%-10d|%-30s%%') which appended a stray '%';
// this implementation writes plain "rank|text" and tolerates junk on read
// (documented deviation D10 in docs/ARCHITECTURE.md).
class ParamHistory {
public:
    ParamHistory();

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
