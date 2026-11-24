#include "core/Localization.h"

#include <windows.h>

#include <cstddef>
#include <iterator>
#include <string_view>
#include <utility>

#include "core/Encoding.h"
#include "core/Logger.h"
#include "core/Text.h"

namespace alt {
namespace {

constexpr long long kMaxLanguageFileBytes = 8 * 1024 * 1024;

struct BuiltinText {
    const char* key;
    const wchar_t* en;
    const wchar_t* zh;
};

// Only the keys the rewrite actually renders are listed here; the original
// language files have 224 keys (docs/spec/02-config-options.md §6.2) that the
// C++ UI does not use yet.  Key names follow the original res* globals; the
// matching legacy INI keys are noted next to each group.
constexpr BuiltinText kBuiltins[] = {
    // Original: [ALTRunForm] EdtShortCutHint / HintList_*.  The rewrite shows
    // one fixed placeholder instead of a random hint.
    {"resHintInput", L"Type a keyword, Enter to run, Esc to hide",
     L"输入关键字，Enter 运行，Esc 关闭"},
    // Original: the edtHint operation line built from KeyToAdd/KeyToRun/etc.
    {"resHintOperation", L"Up/Down Select   Enter Run   Esc Hide   Ctrl+V Paste",
     L"↑↓ 选择   Enter 运行   Esc 关闭   Ctrl+V 粘贴"},
    // Original: no-match text is the empty result list itself; the rewrite
    // prints an explicit placeholder.
    {"resNoMatch", L"No matching shortcut", L"没有匹配项"},
    {"resTitle", L"ALTRun", L"ALTRun"},
    {"resItemCount", L"%d items", L"%d 个条目"},
    // Original: [Common] CanNotExecute.
    {"resCannotExecute", L"Cannot execute the selected shortcut.",
     L"无法执行所选快捷项。"},
    // Original: [Common] About + [AboutForm] Statistics.
    {"resAboutTitle", L"About ALTRun", L"关于 ALTRun"},
    {"resAboutSource", L"Original project: https://github.com/etworker/ALTRun",
     L"原版出处：https://github.com/etworker/ALTRun"},
    {"resAboutRunCount", L"Shortcuts executed: %d", L"已执行快捷项次数：%d"},
    {"resAboutIniPath", L"Settings file: ", L"配置文件："},
    {"resAboutDataDir", L"Data directory: ", L"数据目录："},
    {"resAboutContent", L"About text: ", L"关于文本："},
    {"resAboutContentMissing", L" (file missing, built-in text shown)",
     L"（文件不存在，显示内置文本）"},
    {"resAboutFallback",
     L"ALTRun 2.0 is a C++20 / Win32 / Direct2D rewrite of the original ALTRun.\r\n"
     L"\r\n"
     L"Usage:\r\n"
     L"1. Press the hotkey (default Alt+R) to show the launcher.\r\n"
     L"2. Type a keyword and press Enter to run the shortcut.\r\n"
     L"3. Press Esc to hide the launcher.\r\n"
     L"\r\n"
     L"Original project: https://github.com/etworker/ALTRun",
     L"ALTRun 2.0 是原版 ALTRun 的 C++20 / Win32 / Direct2D 重写版。\r\n"
     L"\r\n"
     L"使用方法：\r\n"
     L"1. 按热键（默认 Alt+R）唤出启动器。\r\n"
     L"2. 输入关键字后回车执行快捷项。\r\n"
     L"3. 按 Esc 隐藏启动器。\r\n"
     L"\r\n"
     L"原版项目：https://github.com/etworker/ALTRun"},
};

std::wstring ExecutableDirectory() {
    wchar_t path[MAX_PATH * 2] = {};
    ::GetModuleFileNameW(nullptr, path, static_cast<DWORD>(std::size(path)));
    std::wstring text = path;
    const size_t slash = text.find_last_of(L"\\/");
    if (slash != std::wstring::npos) {
        text.resize(slash + 1);
    }
    return text;
}

std::wstring DecodeUtf16Le(std::string_view bytes) {
    if (bytes.size() < 2) {
        return {};
    }
    const size_t count = bytes.size() / sizeof(wchar_t);
    return std::wstring(reinterpret_cast<const wchar_t*>(bytes.data()), count);
}

std::wstring DecodeUtf16Be(std::string_view bytes) {
    std::wstring wide;
    wide.reserve(bytes.size() / 2);
    for (size_t i = 0; i + 1 < bytes.size(); i += 2) {
        const auto high = static_cast<unsigned char>(bytes[i]);
        const auto low = static_cast<unsigned char>(bytes[i + 1]);
        wide.push_back(static_cast<wchar_t>((high << 8) | low));
    }
    return wide;
}

}  // namespace

Localization& Localization::Instance() {
    static Localization instance;
    return instance;
}

Localization::Localization() = default;

bool Localization::Load(const std::string& lang) {
    const std::string trimmed(TrimAscii(lang));
    lang_ = trimmed.empty() ? "EN" : trimmed;
    chinese_ = IsChineseLang(lang_);
    file_entries_.clear();

    const std::wstring path = ExecutableDirectory() + ToWide(lang_) + L".lang";
    if (::GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES) {
        // Normal for the built-in English/Chinese fallback: not an error.
        Logger::Instance().Info("localization: " + ToUtf8(path) +
                                " not found, using built-in " +
                                (chinese_ ? "zh" : "en") + " strings");
        return false;
    }

    std::string utf8;
    if (!ReadTextFileUtf8(path, &utf8)) {
        Logger::Instance().Error("localization: cannot read " + ToUtf8(path));
        return false;
    }
    Parse(utf8);
    Logger::Instance().Info("localization: loaded " +
                            std::to_string(file_entries_.size()) +
                            " entries from " + ToUtf8(path));
    return true;
}

std::wstring Localization::Get(const std::string& key) const {
    const std::string lowered = ToLowerUtf8(key);
    const auto loaded = file_entries_.find(lowered);
    if (loaded != file_entries_.end()) {
        return loaded->second;
    }
    for (const BuiltinText& entry : kBuiltins) {
        // Built-in keys use the original res* camelCase spelling; lookups are
        // case-insensitive, matching INI/profile API semantics.
        if (IEqualsAscii(lowered, entry.key)) {
            return chinese_ ? entry.zh : entry.en;
        }
    }
    return ToWide(key);
}

bool Localization::IsChineseLang(const std::string& lang) {
    const std::string lowered = ToLowerUtf8(lang);
    if (lowered.rfind("zh", 0) == 0) {
        return true;
    }
    // 简体中文 / 繁体中文 as plain UTF-8 language names (the legacy ini value).
    return lang.find("中文") != std::string::npos;
}

void Localization::Parse(const std::string& utf8) {
    std::string section;
    size_t start = 0;
    while (start <= utf8.size()) {
        size_t next = utf8.find('\n', start);
        const bool last_line = next == std::string::npos;
        if (last_line) {
            next = utf8.size();
        }
        std::string_view line(utf8.data() + start, next - start);
        start = next + 1;
        if (!line.empty() && line.back() == '\r') {
            line.remove_suffix(1);
        }
        line = TrimAscii(line);
        if (line.empty() || line.front() == ';' || line.front() == '#') {
            if (last_line) {
                break;
            }
            continue;
        }
        if (line.front() == '[') {
            const size_t close = line.find(']');
            if (close != std::string_view::npos) {
                section = ToLowerUtf8(TrimAscii(line.substr(1, close - 1)));
            }
        } else {
            const size_t equals = line.find('=');
            if (equals != std::string_view::npos) {
                const std::string key(
                    ToLowerUtf8(TrimAscii(line.substr(0, equals))));
                const std::wstring value(
                    ToWide(TrimAscii(line.substr(equals + 1))));
                if (!key.empty()) {
                    if (!section.empty()) {
                        file_entries_.emplace(section + "." + key, value);
                    }
                    file_entries_.emplace(key, value);
                }
            }
        }
        if (last_line) {
            break;
        }
    }
}

bool Localization::ReadTextFileUtf8(const std::wstring& path, std::string* utf8) {
    if (utf8 == nullptr || path.empty()) {
        return false;
    }
    utf8->clear();

    HANDLE file = ::CreateFileW(path.c_str(), GENERIC_READ,
                                FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                                OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }
    LARGE_INTEGER size = {};
    if (::GetFileSizeEx(file, &size) == FALSE || size.QuadPart < 0 ||
        size.QuadPart > kMaxLanguageFileBytes) {
        ::CloseHandle(file);
        return false;
    }
    std::string bytes(static_cast<size_t>(size.QuadPart), '\0');
    if (!bytes.empty()) {
        DWORD read = 0;
        if (::ReadFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &read,
                       nullptr) == FALSE) {
            ::CloseHandle(file);
            return false;
        }
        bytes.resize(read);
    }
    ::CloseHandle(file);

    // Encoding detection matches ShortcutStore::Load()/the manager import:
    // BOM first, then strict UTF-8, otherwise the legacy ANSI/GBK code page.
    if (bytes.size() >= 3 && static_cast<unsigned char>(bytes[0]) == 0xEF &&
        static_cast<unsigned char>(bytes[1]) == 0xBB &&
        static_cast<unsigned char>(bytes[2]) == 0xBF) {
        bytes.erase(0, 3);
        *utf8 = std::move(bytes);
        return true;
    }
    if (bytes.size() >= 2 && static_cast<unsigned char>(bytes[0]) == 0xFF &&
        static_cast<unsigned char>(bytes[1]) == 0xFE) {
        *utf8 = ToUtf8(DecodeUtf16Le(
            std::string_view(bytes).substr(2)));
        return true;
    }
    if (bytes.size() >= 2 && static_cast<unsigned char>(bytes[0]) == 0xFE &&
        static_cast<unsigned char>(bytes[1]) == 0xFF) {
        *utf8 = ToUtf8(DecodeUtf16Be(
            std::string_view(bytes).substr(2)));
        return true;
    }
    if (IsValidUtf8(bytes)) {
        *utf8 = std::move(bytes);
    } else {
        *utf8 = AnsiToUtf8(bytes);
    }
    return true;
}

}  // namespace alt
