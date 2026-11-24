#pragma once

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "core/Shortcut.h"

namespace alt {

// Owns the shortcut list, the favourites map and their persistence.
//
// Files (same locations as the original, next to the executable):
//   ShortCutList.txt   items, ANSI/GBK, CRLF, `F%-8d|%-20s|%-30s|%-30s|%s`
//   FavoriteList.txt   `keyword(lower)|name`, one per line
//   ShortCutList.txt.bak  backup written on successful save
class ShortcutStore {
public:
    ShortcutStore();

    // Reads ShortCutList.txt + FavoriteList.txt. Returns true when a list file
    // was found and parsed (a missing file is not an error: the caller seeds
    // the default list).
    bool Load();
    bool Save() const;

    const std::vector<ShortcutEntry>& entries() const { return entries_; }
    std::vector<ShortcutEntry>& mutable_entries() { return entries_; }

    // Number of non-blank entries.
    size_t item_count() const;

    // Parses one line of ShortCutList.txt. Returns false for comment/invalid
    // rows (scOther). Exposed for migration and tests.
    static bool ParseLine(std::string_view line, ShortcutEntry* entry);

    // Serializes one entry (including the trailing blank line case).
    static std::string FormatEntry(const ShortcutEntry& entry);

    // Entry mutations (index refers to entries(), blank rows included).
    bool RemoveEntry(size_t index);
    bool TouchEntry(size_t index);  // Freq += 1

    // Favourites: key = lowercase search keyword, value = item Name.
    void RememberFavourite(std::string_view keyword, std::string_view name);
    // Name previously launched for |keyword|, empty when unknown.
    std::string FindFavourite(std::string_view keyword) const;
    const std::vector<std::pair<std::string, std::string>>& favourites() const {
        return favourites_;
    }

    // Recent list (original [DEBUG] LatestList): a comma separated list of
    // ShortCutList row indices, at most 10 entries, most recent first.
    void SetLatestFromSpec(std::string_view spec);
    std::string LatestIndexSpec() const;
    void RecordLatest(size_t entry_index);
    const std::vector<size_t>& latest() const { return latest_; }

    // Writes the built-in default list when ShortCutList.txt does not exist
    // (original behaviour: regenerated whenever the file is deleted).
    void SeedDefaultsIfMissing();

    const std::wstring& list_path() const { return list_path_; }
    const std::wstring& favourite_path() const { return favourite_path_; }

    // Executable directory (all relative paths resolve against it, like the
    // original launcher which lives in the same folder as its data).
    static std::wstring ExeDirectory();

private:
    static std::wstring ReadAllBytes(const std::wstring& path, bool* ok);
    static bool WriteAllBytes(const std::wstring& path, std::string_view bytes);

    std::vector<ShortcutEntry> entries_;
    std::vector<std::pair<std::string, std::string>> favourites_;
    std::vector<size_t> latest_;
    std::wstring list_path_;
    std::wstring favourite_path_;
};

}  // namespace alt
