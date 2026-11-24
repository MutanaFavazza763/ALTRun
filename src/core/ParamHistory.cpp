#include "core/ParamHistory.h"

#include <windows.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>

#include "core/Encoding.h"
#include "core/Logger.h"
#include "core/ShortcutStore.h"
#include "core/Text.h"

namespace alt {
namespace {

constexpr char kParamHistoryFile[] = "ParamHistory.txt";

std::string TrimDelphi(std::string_view text) {
    size_t begin = 0;
    size_t end = text.size();
    while (begin < end && static_cast<unsigned char>(text[begin]) <= ' ') ++begin;
    while (end > begin && static_cast<unsigned char>(text[end - 1]) <= ' ') --end;
    return std::string(text.substr(begin, end - begin));
}

bool ReadBytes(const std::wstring& path, std::string* bytes) {
    bytes->clear();
    HANDLE file = ::CreateFileW(path.c_str(), GENERIC_READ,
                                FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                                OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }
    LARGE_INTEGER size = {};
    ::GetFileSizeEx(file, &size);
    if (size.QuadPart > 0) {
        bytes->resize(static_cast<size_t>(size.QuadPart));
        DWORD read = 0;
        ::ReadFile(file, bytes->data(), static_cast<DWORD>(size.QuadPart), &read,
                   nullptr);
        bytes->resize(read);
    }
    ::CloseHandle(file);
    return true;
}

bool WriteBytes(const std::wstring& path, std::string_view bytes) {
    const std::wstring temp = path + L".tmp";
    HANDLE file = ::CreateFileW(temp.c_str(), GENERIC_WRITE, 0, nullptr,
                                CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }
    DWORD written = 0;
    const BOOL ok = ::WriteFile(file, bytes.data(), static_cast<DWORD>(bytes.size()),
                                &written, nullptr);
    ::FlushFileBuffers(file);
    ::CloseHandle(file);
    if (!ok) {
        ::DeleteFileW(temp.c_str());
        return false;
    }
    if (!::MoveFileExW(temp.c_str(), path.c_str(),
                       MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        ::DeleteFileW(temp.c_str());
        return false;
    }
    return true;
}

}  // namespace

ParamHistory::ParamHistory() {
    path_ = ShortcutStore::ExeDirectory() + ToWide(kParamHistoryFile);
}

ParamHistory::ParamHistory(std::wstring path) : path_(std::move(path)) {}

bool ParamHistory::Load(int limit) {
    items_.clear();
    std::string bytes;
    if (!ReadBytes(path_, &bytes) || bytes.empty()) {
        return false;
    }
    const std::string utf8 =
        IsValidUtf8(bytes) ? bytes : AnsiToUtf8(bytes);  // legacy GBK

    size_t start = 0;
    while (start <= utf8.size() && static_cast<int>(items_.size()) < limit) {
        size_t next = utf8.find('\n', start);
        if (next == std::string::npos) {
            next = utf8.size();
        }
        std::string line = utf8.substr(start, next - start);
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        const size_t bar = line.find('|');
        if (bar != std::string::npos) {
            const std::string rank_text = TrimDelphi(line.substr(0, bar));
            std::string text = TrimDelphi(line.substr(bar + 1));
            // A trailing '%' may be valid user input (100%, %PATH%).
            // The legacy writer's behaviour is uncertain; never guess and
            // destructively strip characters from persisted parameters.
            if (!text.empty()) {
                ParamHistoryItem item;
                item.rank = std::atoi(rank_text.c_str());
                item.text = text;
                const bool duplicate =
                    std::any_of(items_.begin(), items_.end(),
                                [&text](const ParamHistoryItem& existing) {
                                    return existing.text == text;
                                });
                if (!duplicate) {
                    items_.push_back(std::move(item));
                }
            }
        }
        if (next == utf8.size()) {
            break;
        }
        start = next + 1;
    }
    Logger::Instance().Info("param history: " +
                            std::to_string(items_.size()) + " entries");
    return true;
}

bool ParamHistory::Save(int limit) const {
    std::string utf8;
    int written = 0;
    for (const ParamHistoryItem& item : items_) {
        if (written >= limit) {
            break;
        }
        utf8 += std::to_string(item.rank);
        utf8 += '|';
        utf8 += item.text;
        utf8 += "\r\n";
        ++written;
    }
    // Original file is ANSI; keep it readable by the old binary.
    return WriteBytes(path_, Utf8ToAnsi(utf8));
}

void ParamHistory::Record(std::string_view text, int limit) {
    const std::string value = TrimDelphi(text);
    if (value.empty()) {
        return;
    }

    auto it = std::find_if(items_.begin(), items_.end(),
                           [&value](const ParamHistoryItem& item) {
                               return item.text == value;
                           });
    if (it != items_.end()) {
        ++it->rank;
        return;
    }

    if (static_cast<int>(items_.size()) >= limit && !items_.empty()) {
        // Evict the smallest rank, scanning from the tail like the original.
        auto smallest = items_.end() - 1;
        for (auto candidate = items_.end() - 1; candidate >= items_.begin();
             --candidate) {
            if (candidate->rank < smallest->rank) {
                smallest = candidate;
            }
            if (candidate == items_.begin()) {
                break;
            }
        }
        items_.erase(smallest);
    }

    ParamHistoryItem item;
    item.rank = 0;
    item.text = value;
    items_.insert(items_.begin(), std::move(item));
}

}  // namespace alt
