#include "core/ShortcutStore.h"

#include <windows.h>

#include <algorithm>
#include <string>
#include <vector>

#include "core/Encoding.h"
#include "core/Logger.h"
#include "core/Text.h"

namespace alt {
namespace {

constexpr char kListFileName[] = "ShortCutList.txt";
constexpr char kFavouriteFileName[] = "FavoriteList.txt";

// Delphi Style: Trim removes every leading/trailing character <= ' '.
std::string TrimDelphi(std::string_view text) {
    size_t begin = 0;
    size_t end = text.size();
    while (begin < end && static_cast<unsigned char>(text[begin]) <= ' ') {
        ++begin;
    }
    while (end > begin && static_cast<unsigned char>(text[end - 1]) <= ' ') {
        --end;
    }
    return std::string(text.substr(begin, end - begin));
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

std::vector<std::string> SplitKeepAll(std::string_view text, char delimiter) {
    std::vector<std::string> parts;
    size_t start = 0;
    while (start <= text.size()) {
        const size_t next = text.find(delimiter, start);
        if (next == std::string_view::npos) {
            parts.emplace_back(text.substr(start));
            break;
        }
        parts.emplace_back(text.substr(start, next - start));
        start = next + 1;
    }
    return parts;
}

bool IsAllDigits(std::string_view text) {
    if (text.empty()) {
        return false;
    }
    return std::all_of(text.begin(), text.end(), [](char c) {
        return c >= '0' && c <= '9';
    });
}

// Pads a UTF-8 field with spaces until its ANSI (GBK) representation is at
// least |width| bytes, mirroring Delphi's Format('%-20s', ...) on AnsiString.
std::string PadField(const std::string& utf8, size_t width) {
    const std::string ansi = Utf8ToAnsi(utf8);
    if (ansi.size() >= width) {
        return utf8;  // original never truncates
    }
    std::string padded = utf8;
    padded.append(width - ansi.size(), ' ');
    return padded;
}

std::string LowerAscii(std::string_view text) {
    std::string out(text);
    for (char& c : out) {
        if (c >= 'A' && c <= 'Z') {
            c = static_cast<char>(c - 'A' + 'a');
        }
    }
    return out;
}

std::string JoinFrom(const std::vector<std::string>& parts, size_t first,
                     char delimiter) {
    std::string joined;
    for (size_t i = first; i < parts.size(); ++i) {
        if (i != first) {
            joined.push_back(delimiter);
        }
        joined += parts[i];
    }
    return joined;
}

}  // namespace

ShortcutStore::ShortcutStore() {
    const std::wstring dir = ExeDirectory();
    list_path_ = dir + ToWide(kListFileName);
    favourite_path_ = dir + ToWide(kFavouriteFileName);
}

std::wstring ShortcutStore::ExeDirectory() {
    wchar_t buffer[MAX_PATH * 2] = {};
    ::GetModuleFileNameW(nullptr, buffer, static_cast<DWORD>(std::size(buffer)));
    std::wstring path = buffer;
    const size_t slash = path.find_last_of(L"\\/");
    if (slash != std::wstring::npos) {
        path.resize(slash + 1);
    }
    return path;
}

std::wstring ShortcutStore::ReadAllBytes(const std::wstring& path, bool* ok) {
    std::wstring text;
    HANDLE file = ::CreateFileW(path.c_str(), GENERIC_READ,
                                FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                                OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        if (ok) *ok = false;
        return text;
    }
    LARGE_INTEGER size = {};
    ::GetFileSizeEx(file, &size);
    if (size.QuadPart > 0) {
        text.resize(static_cast<size_t>(size.QuadPart) / sizeof(wchar_t));
        DWORD read = 0;
        ::ReadFile(file, text.data(), static_cast<DWORD>(size.QuadPart), &read,
                   nullptr);
        text.resize(read / sizeof(wchar_t));
    }
    ::CloseHandle(file);
    if (ok) *ok = true;
    return text;
}

namespace {
// Reads raw bytes (unlike the wide helper above).
bool ReadFileBytes(const std::wstring& path, std::string* bytes) {
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
}  // namespace

bool ShortcutStore::WriteAllBytes(const std::wstring& path, std::string_view bytes) {
    // Write to a temp file first and replace atomically; the original was
    // non-atomic (a crash could truncate ShortCutList.txt), which the rewrite
    // deliberately fixes.
    const std::wstring temp = path + L".tmp";
    HANDLE file = ::CreateFileW(temp.c_str(), GENERIC_WRITE, 0, nullptr,
                                CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        Logger::Instance().Error("cannot create " + ToUtf8(temp));
        return false;
    }
    DWORD written = 0;
    const BOOL ok = ::WriteFile(file, bytes.data(),
                                static_cast<DWORD>(bytes.size()), &written, nullptr);
    ::FlushFileBuffers(file);
    ::CloseHandle(file);
    if (!ok || written != bytes.size()) {
        ::DeleteFileW(temp.c_str());
        return false;
    }

    // Keep one backup of the previous content, like the original .bak file.
    const std::wstring backup = path + L".bak";
    ::CopyFileW(path.c_str(), backup.c_str(), FALSE);

    if (!::MoveFileExW(temp.c_str(), path.c_str(),
                       MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        ::DeleteFileW(temp.c_str());
        return false;
    }
    return true;
}

bool ShortcutStore::ParseLine(std::string_view raw_line, ShortcutEntry* entry) {
    if (entry == nullptr) {
        return false;
    }
    *entry = ShortcutEntry{};

    std::string line = TrimDelphi(raw_line);
    // Full-width comma was accepted by the legacy parser.
    ReplaceAll(line, "\xEF\xBC\x8C", ",");

    if (line.empty()) {
        entry->is_blank = true;
        return true;
    }

    // The pipe format always carries at least four separators. Choosing by
    // separator presence (instead of "first char is F") fixes defect P1 from
    // the original parser, where old comma lines starting with F were dropped.
    const size_t pipes = static_cast<size_t>(
        std::count(line.begin(), line.end(), '|'));
    std::vector<std::string> parts;
    char delimiter = '|';
    if (pipes >= 3) {
        parts = SplitKeepAll(line, '|');
    } else {
        delimiter = ',';
        parts = SplitKeepAll(line, ',');
    }

    if (parts.size() < 3) {
        // scOther / unparsable row: dropped from the model, like the original.
        entry->item.name = line;
        return false;
    }

    size_t offset = 0;
    const std::string first = TrimDelphi(parts[0]);
    if (!first.empty() && (first[0] == 'F' || first[0] == 'f')) {
        const std::string digits = TrimDelphi(std::string_view(first).substr(1));
        int freq = 0;
        if (IsAllDigits(digits)) {
            freq = digits.size() > 8 ? 10000 : std::atoi(digits.c_str());
        }
        entry->item.freq = freq;
        offset = 1;
    }

    if (offset < parts.size()) {
        ParamType type = ParamType::None;
        if (ParamTypeTryParse(parts[offset], &type)) {
            entry->item.param_type = type;
            ++offset;
        }
    }

    if (offset + 1 >= parts.size()) {
        return false;
    }
    entry->item.keyword = TrimDelphi(parts[offset]);
    entry->item.name = TrimDelphi(parts[offset + 1]);
    // Everything else is the command line. Re-joining (instead of the legacy
    // "delete already parsed substrings" trick) removes defects P2/P3.
    entry->item.command_line = TrimDelphi(JoinFrom(parts, offset + 2, delimiter));
    return true;
}

std::string ShortcutStore::FormatEntry(const ShortcutEntry& entry) {
    if (entry.is_blank) {
        return {};
    }
    const Shortcut& item = entry.item;
    std::string line;
    line += 'F';
    line += PadField(std::to_string(item.freq), 8);
    line += '|';
    line += PadField(ParamTypeToStorage(item.param_type), 20);
    line += '|';
    line += PadField(item.keyword, 30);
    line += '|';
    line += PadField(item.name, 30);
    line += '|';
    line += item.command_line;
    return line;
}

bool ShortcutStore::Load() {
    entries_.clear();
    favourites_.clear();

    std::string bytes;
    const bool found = ReadFileBytes(list_path_, &bytes);
    if (found && !bytes.empty()) {
        std::string utf8;
        if (bytes.size() >= 3 && static_cast<unsigned char>(bytes[0]) == 0xEF &&
            static_cast<unsigned char>(bytes[1]) == 0xBB &&
            static_cast<unsigned char>(bytes[2]) == 0xBF) {
            utf8 = bytes.substr(3);
        } else if (bytes.size() >= 2 && static_cast<unsigned char>(bytes[0]) == 0xFF &&
                   static_cast<unsigned char>(bytes[1]) == 0xFE) {
            const std::wstring wide(reinterpret_cast<const wchar_t*>(bytes.data() + 2),
                                    (bytes.size() - 2) / sizeof(wchar_t));
            utf8 = ToUtf8(wide);
        } else if (IsValidUtf8(bytes)) {
            utf8 = bytes;
        } else {
            utf8 = AnsiToUtf8(bytes);  // legacy GBK/ANSI
            Logger::Instance().Info("shortcut list decoded as ANSI/GBK");
        }

        size_t start = 0;
        // Iterate real lines only: a trailing CRLF must not create an extra
        // empty segment (that bug made every load/save cycle append a blank
        // row to ShortCutList.txt).
        while (start < utf8.size()) {
            size_t next = utf8.find('\n', start);
            if (next == std::string::npos) {
                next = utf8.size();
            }
            std::string line = utf8.substr(start, next - start);
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            ShortcutEntry entry;
            if (ParseLine(line, &entry)) {
                entries_.push_back(std::move(entry));
            }
            if (next == utf8.size()) {
                break;
            }
            start = next + 1;
        }
        Logger::Instance().Info("loaded " + std::to_string(item_count()) +
                                " shortcuts from " + ToUtf8(list_path_));
    }

    std::string favourite_bytes;
    if (ReadFileBytes(favourite_path_, &favourite_bytes) && !favourite_bytes.empty()) {
        const std::string utf8 = IsValidUtf8(favourite_bytes)
                                     ? favourite_bytes
                                     : AnsiToUtf8(favourite_bytes);
        size_t start = 0;
        // Same trailing-CRLF guard as the shortcut list above.
        while (start < utf8.size()) {
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
                const std::string key = LowerAscii(TrimDelphi(line.substr(0, bar)));
                const std::string value = TrimDelphi(line.substr(bar + 1));
                if (!key.empty()) {
                    bool replaced = false;
                    for (auto& pair : favourites_) {
                        if (pair.first == key) {
                            pair.second = value;
                            replaced = true;
                            break;
                        }
                    }
                    if (!replaced) {
                        favourites_.emplace_back(key, value);
                    }
                }
            }
            if (next == utf8.size()) {
                break;
            }
            start = next + 1;
        }
    }

    return found;
}

bool ShortcutStore::Save() const {
    std::string utf8_content;
    for (const ShortcutEntry& entry : entries_) {
        utf8_content += FormatEntry(entry);
        utf8_content += "\r\n";
    }
    const std::string ansi = Utf8ToAnsi(utf8_content);
    if (!WriteAllBytes(list_path_, ansi)) {
        Logger::Instance().Error("cannot save shortcut list");
        return false;
    }

    std::string favourites_content;
    for (const auto& pair : favourites_) {
        std::string line = PadField(pair.first, 30);
        line += '|';
        line += PadField(pair.second, 30);
        favourites_content += line;
        favourites_content += "\r\n";
    }
    if (!WriteAllBytes(favourite_path_, Utf8ToAnsi(favourites_content))) {
        Logger::Instance().Warn("cannot save favourite list");
    }
    return true;
}

size_t ShortcutStore::item_count() const {
    return static_cast<size_t>(
        std::count_if(entries_.begin(), entries_.end(),
                      [](const ShortcutEntry& entry) { return !entry.is_blank; }));
}

bool ShortcutStore::RemoveEntry(size_t index) {
    if (index >= entries_.size()) {
        return false;
    }
    entries_.erase(entries_.begin() + static_cast<std::ptrdiff_t>(index));
    return true;
}

bool ShortcutStore::TouchEntry(size_t index) {
    if (index >= entries_.size() || entries_[index].is_blank) {
        return false;
    }
    ++entries_[index].item.freq;
    return true;
}

void ShortcutStore::RememberFavourite(std::string_view keyword, std::string_view name) {
    const std::string key = LowerAscii(std::string(TrimDelphi(keyword)));
    if (key.empty()) {
        return;
    }
    for (auto& pair : favourites_) {
        if (pair.first == key) {
            pair.second = std::string(name);
            return;
        }
    }
    favourites_.emplace_back(key, std::string(name));
}

std::string ShortcutStore::FindFavourite(std::string_view keyword) const {
    const std::string key = LowerAscii(std::string(TrimDelphi(keyword)));
    for (const auto& pair : favourites_) {
        if (pair.first == key) {
            return pair.second;
        }
    }
    return {};
}

namespace {
std::string TrimSpaces(std::string_view text) {
    size_t begin = 0;
    size_t end = text.size();
    while (begin < end && (text[begin] == ' ' || text[begin] == '\t')) ++begin;
    while (end > begin && (text[end - 1] == ' ' || text[end - 1] == '\t')) --end;
    return std::string(text.substr(begin, end - begin));
}
}  // namespace

void ShortcutStore::SetLatestFromSpec(std::string_view spec) {
    latest_.clear();
    size_t start = 0;
    while (start <= spec.size()) {
        const size_t comma = spec.find(',', start);
        const std::string piece =
            TrimSpaces(spec.substr(start, comma == std::string_view::npos
                                             ? std::string_view::npos
                                             : comma - start));
        if (!piece.empty()) {
            const long long index = std::atoll(piece.c_str());
            if (index >= 0 && index < static_cast<long long>(entries_.size())) {
                const size_t value = static_cast<size_t>(index);
                const bool duplicate =
                    std::find(latest_.begin(), latest_.end(), value) != latest_.end();
                if (!duplicate && latest_.size() < 10) {
                    latest_.push_back(value);
                }
            }
        }
        if (comma == std::string_view::npos) {
            break;
        }
        start = comma + 1;
    }
}

std::string ShortcutStore::LatestIndexSpec() const {
    std::string spec;
    for (size_t i = 0; i < latest_.size(); ++i) {
        if (i != 0) {
            spec.push_back(',');
        }
        spec += std::to_string(latest_[i]);
    }
    return spec;
}

void ShortcutStore::RecordLatest(size_t entry_index) {
    if (entry_index >= entries_.size()) {
        return;
    }
    auto it = std::find(latest_.begin(), latest_.end(), entry_index);
    if (it != latest_.end()) {
        latest_.erase(it);
    }
    latest_.insert(latest_.begin(), entry_index);
    if (latest_.size() > 10) {
        latest_.resize(10);
    }
}

void ShortcutStore::SeedDefaultsIfMissing() {
    if (::GetFileAttributesW(list_path_.c_str()) != INVALID_FILE_ATTRIBUTES) {
        return;
    }
    if (!entries_.empty()) {
        return;
    }

    auto add = [this](std::string keyword, std::string name, ParamType type,
                      std::string command_line, int freq) {
        ShortcutEntry entry;
        entry.item.param_type = type;
        entry.item.keyword = std::move(keyword);
        entry.item.name = std::move(name);
        entry.item.command_line = std::move(command_line);
        entry.item.freq = freq;
        entries_.push_back(std::move(entry));
    };
    auto blank = [this]() {
        ShortcutEntry entry;
        entry.is_blank = true;
        entries_.push_back(std::move(entry));
    };

    // Mirrors the built-in list of the original launcher
    // (docs/spec/03-shortcut-model.md §9.3). WinCtl.exe based entries are kept
    // for fidelity even though that helper binary is not shipped.
    add("Computer", "我的电脑", ParamType::None,
        "::{20D04FE0-3AEA-1069-A2D8-08002B30309D}", 100);
    add("Explorer", "资源管理器", ParamType::None,
        "explorer.exe /e,::{20D04FE0-3AEA-1069-A2D8-08002B30309D}", 0);
    add("MyDocument", "我的文档", ParamType::None, "explorer.exe", 0);
    add("ie", "IE浏览器", ParamType::None, "iexplore.exe", 8);
    add("notepad", "记事本", ParamType::None, "notepad", 7);
    add("mspaint", "画图", ParamType::None, "mspaint", 0);
    add("cmd", "Dos窗口", ParamType::NoEncoding, "cmd /k %p", 6);
    add("Calc", "计算器", ParamType::None, "calc", 5);
    add("msconfig", "系统配置", ParamType::None, "msconfig", 0);
    add("Reg", "注册表编辑器", ParamType::None, "regedt32", 4);
    add("myip", "我的IP地址", ParamType::None, "nslookup", 0);
    add("dev", "设备管理器", ParamType::None, "devmgmt.msc", 0);
    add("Taskmgr", "任务管理器", ParamType::None, "taskmgr", 0);
    add("ShutDown", "立即关机", ParamType::None, "shutdown /s /t 5", 0);
    add("Reboot", "立即重启", ParamType::None, "shutdown /r /t 5", 0);
    blank();
    add("Windows", "Windows 目录", ParamType::None, "%WINDIR%", 0);
    add("System32", "System32 目录", ParamType::None, "%WINDIR%\\System32", 0);
    add("ProgramFiles", "Program Files 目录", ParamType::None, "%PROGRAMFILES%", 0);
    add("Recent", "最近打开文件 目录", ParamType::None, "%USERPROFILE%\\Recent", 10);
    blank();
    add("b", "百度 搜索引擎", ParamType::UrlQuery, "http://www.baidu.com/s?wd=", 12);
    add("g", "Google 搜索引擎", ParamType::Utf8Query,
        "http://www.google.com/search?q=", 14);
    add("s", "搜狗MP3 搜索引擎", ParamType::UrlQuery,
        "http://mp3.sogou.com/music.so?query=", 0);
    add("zd", "百度知道 搜索引擎", ParamType::UrlQuery,
        "http://zhidao.baidu.com/q?ct=17&pn=0&tn=ikaslist&rn=10&word=", 9);
    add("v", "VeryCD 搜索引擎", ParamType::Utf8Query,
        "http://www.verycd.com/search/folders?kw=", 0);
    add("y", "Yahoo 搜索引擎", ParamType::Utf8Query,
        "http://search.yahoo.com/search?p=", 0);
    blank();
    add("Upgrade", "前往升级网址", ParamType::None,
        "http://journeyboy.blog.sohu.com/112820379.html", 15);
    add("Config", "编辑配置", ParamType::None, ".\\ALTRun.ini", 0);
    blank();
    add("AddRemoveProgram", "添加/删除程序", ParamType::None, "appwiz.cpl", 0);
    add("ControlPanel", "控制面板", ParamType::None, "control.exe", 0);
    add("InternetConnection", "Internet控制", ParamType::None,
        "control.exe Inetcpl.cpl ,4", 0);
    add("SystemProperty", "系统属性", ParamType::None, "Sysdm.cpl", 0);
    add("r", "运行", ParamType::NoEncoding, "{%p}", 100);
    add("cb", "百度 搜索剪贴板", ParamType::UrlQuery,
        "http://www.baidu.com/s?wd={%c}", 12);
    add("cg", "Google 搜索剪贴板", ParamType::Utf8Query,
        "http://www.google.com/search?q={%c}", 24);

    Save();
    Logger::Instance().Info("seeded default shortcut list");
}

}  // namespace alt
