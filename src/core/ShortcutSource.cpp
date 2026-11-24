#include "core/ShortcutSource.h"

#include <windows.h>

#include <objbase.h>
#include <shlobj.h>

#include <string>
#include <string_view>

#include "core/ShortcutStore.h"
#include "core/Text.h"

namespace alt {
namespace {

constexpr wchar_t kLinkExtension[] = L".lnk";
constexpr std::string_view kHttpPrefix = "http://";
constexpr std::string_view kHttpsPrefix = "https://";
constexpr std::string_view kAppUrlPrefix = "--app=";

wchar_t FoldAsciiWide(wchar_t c) {
    if (c >= L'A' && c <= L'Z') {
        return static_cast<wchar_t>(c - L'A' + L'a');
    }
    return c;
}

bool WideStartsWithNoCase(std::wstring_view text, std::wstring_view prefix) {
    if (text.size() < prefix.size()) {
        return false;
    }
    for (size_t i = 0; i < prefix.size(); ++i) {
        if (FoldAsciiWide(text[i]) != FoldAsciiWide(prefix[i])) {
            return false;
        }
    }
    return true;
}

bool StartsWithNoCase(std::string_view text, std::string_view prefix) {
    if (text.size() < prefix.size()) {
        return false;
    }
    return IEqualsAscii(text.substr(0, prefix.size()), prefix);
}

bool EndsWithNoCase(std::string_view text, std::string_view suffix) {
    if (text.size() < suffix.size()) {
        return false;
    }
    return IEqualsAscii(text.substr(text.size() - suffix.size()), suffix);
}

size_t FindNoCase(std::string_view haystack, std::string_view needle) {
    if (needle.empty() || haystack.size() < needle.size()) {
        return std::string_view::npos;
    }
    for (size_t i = 0; i + needle.size() <= haystack.size(); ++i) {
        if (IEqualsAscii(haystack.substr(i, needle.size()), needle)) {
            return i;
        }
    }
    return std::string_view::npos;
}

bool IsHttpUrl(std::string_view text) {
    return StartsWithNoCase(text, kHttpPrefix) || StartsWithNoCase(text, kHttpsPrefix);
}

bool HasPathSeparatorAtEnd(std::string_view text) {
    return !text.empty() && (text.back() == '\\' || text.back() == '/');
}

// Leaf name of a path, ignoring trailing separators ("C:\a\b\" -> "b").
std::string LeafComponent(std::string_view path) {
    size_t end = path.size();
    while (end > 0 && (path[end - 1] == '\\' || path[end - 1] == '/')) {
        --end;
    }
    if (end == 0) {
        return {};
    }
    const size_t separator = path.find_last_of("\\/", end - 1);
    const size_t start = separator == std::string_view::npos ? 0 : separator + 1;
    return std::string(path.substr(start, end - start));
}

// Removes the last extension (".exe"), never the whole name (".gitignore"
// keeps its dot because there is no stem left).
std::string StripExtension(std::string_view name) {
    const size_t dot = name.find_last_of('.');
    if (dot == std::string_view::npos || dot == 0) {
        return std::string(name);
    }
    return std::string(name.substr(0, dot));
}

std::wstring StripSurroundingQuotes(std::wstring text) {
    if (text.size() >= 2 && text.front() == L'"' && text.back() == L'"') {
        text = text.substr(1, text.size() - 2);
    }
    return text;
}

bool FileOrDirectoryExists(const std::string& utf8_path) {
    if (utf8_path.empty()) {
        return false;
    }
    const std::wstring wide = ToWide(utf8_path);
    if (wide.empty()) {
        return false;
    }
    return ::GetFileAttributesW(wide.c_str()) != INVALID_FILE_ATTRIBUTES;
}

bool IsDirectory(const std::string& utf8_path) {
    if (utf8_path.empty()) {
        return false;
    }
    const std::wstring wide = ToWide(utf8_path);
    if (wide.empty()) {
        return false;
    }
    const DWORD attributes = ::GetFileAttributesW(wide.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        return false;
    }
    return (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

// Host part of an URL: text after "://" up to the first '/', '?' or '#'.
std::string UrlHost(std::string_view url) {
    const size_t scheme = url.find("://");
    if (scheme == std::string_view::npos) {
        return {};
    }
    const size_t start = scheme + 3;
    size_t end = url.find_first_of("/?#", start);
    if (end == std::string_view::npos) {
        end = url.size();
    }
    return std::string(url.substr(start, end - start));
}

// First blank-free token of a command line; surrounding double quotes are
// removed when the token is quoted.
std::string FirstToken(std::string_view command_line) {
    const std::string_view trimmed = TrimAscii(command_line);
    if (trimmed.empty()) {
        return {};
    }
    if (trimmed.front() == '"') {
        const size_t closing = trimmed.find('"', 1);
        if (closing == std::string_view::npos) {
            return std::string(trimmed.substr(1));
        }
        return std::string(trimmed.substr(1, closing - 1));
    }
    const size_t space = trimmed.find_first_of(" \t");
    if (space == std::string_view::npos) {
        return std::string(trimmed);
    }
    return std::string(trimmed.substr(0, space));
}

std::string WindowsDirectoryUtf8() {
    wchar_t directory[MAX_PATH] = {};
    const UINT length =
        ::GetWindowsDirectoryW(directory, static_cast<UINT>(std::size(directory)));
    if (length == 0 || length >= std::size(directory)) {
        return {};
    }
    return ToUtf8(std::wstring(directory, length));
}

// True when the resolved target is one of the "%windir%\Installer\{...}"
// icon stubs that .lnk files created by Windows Installer point at.
bool LooksLikeInstallerIconStub(const std::string& command_line_utf8) {
    const std::string target = FirstToken(command_line_utf8);
    const std::string windows = WindowsDirectoryUtf8();
    if (target.empty() || windows.empty()) {
        return false;
    }
    if (!StartsWithNoCase(target, windows)) {
        return false;
    }
    std::string rest = target.substr(windows.size());
    if (rest.empty() || (rest.front() != '\\' && rest.front() != '/')) {
        return false;
    }
    rest.erase(0, 1);
    return StartsWithNoCase(rest, "installer\\") || StartsWithNoCase(rest, "installer/");
}

// Keyword from a path: the leaf without extension for files, the full leaf for
// directories and for paths that clearly end with a separator.
std::string KeywordFromPath(std::string_view path) {
    std::string text(path);
    if (text.size() >= 2 && text.front() == '"' && text.back() == '"') {
        text = std::string(TrimAscii(std::string_view(text).substr(1, text.size() - 2)));
    }
    if (text.empty()) {
        return {};
    }
    const std::string leaf = LeafComponent(text);
    if (leaf.empty()) {
        return text;
    }
    if (HasPathSeparatorAtEnd(text) || IsDirectory(text)) {
        return leaf;
    }
    return StripExtension(leaf);
}

}  // namespace

Shortcut ShortcutFromPath(const std::string& path_utf8) {
    Shortcut item;
    std::string path = std::string(TrimAscii(path_utf8));
    if (path.size() >= 2 && path.front() == '"' && path.back() == '"') {
        path = std::string(TrimAscii(std::string_view(path).substr(1, path.size() - 2)));
    }
    if (path.empty()) {
        return item;
    }

    if (IsHttpUrl(path)) {
        return ShortcutFromUrl(path);
    }

    const std::string leaf = LeafComponent(path);

    // A shortcut resolves to its target; installer icon stubs and broken links
    // keep the .lnk path (legacy behaviour, docs/spec/03 §8.1).
    if (EndsWithNoCase(path, ToUtf8(kLinkExtension))) {
        const std::string resolved = ResolveLnk(ToWide(path));
        if (!resolved.empty() && !LooksLikeInstallerIconStub(resolved)) {
            const std::string target = FirstToken(resolved);
            item.command_line = resolved;
            item.keyword = KeywordFromPath(target);
            if (item.keyword.empty()) {
                item.keyword = StripExtension(leaf);
            }
            item.name = item.keyword;
            return item;
        }
        item.command_line = path;
        item.keyword = StripExtension(leaf);
        item.name = item.keyword;
        return item;
    }

    item.command_line = path;
    item.keyword = KeywordFromPath(path);
    item.name = item.keyword;
    return item;
}

Shortcut ShortcutFromUrl(const std::string& url_utf8) {
    Shortcut item;
    const std::string url = std::string(TrimAscii(url_utf8));
    if (url.empty()) {
        return item;
    }
    item.command_line = url;
    item.keyword = UrlHost(url);
    item.name = item.keyword;
    return item;
}

Shortcut ShortcutFromCommandLine(const std::string& command_line_utf8) {
    Shortcut item;
    const std::string command = std::string(TrimAscii(command_line_utf8));
    if (command.empty()) {
        return item;
    }

    // Chrome/Edge "app mode" shortcuts: chrome.exe --app=https://host/...
    const size_t app_prefix = FindNoCase(command, kAppUrlPrefix);
    if (app_prefix != std::string::npos) {
        std::string target = command.substr(app_prefix + kAppUrlPrefix.size());
        while (!target.empty() && (target.back() == '"' || target.back() == ' ')) {
            target.pop_back();
        }
        if (IsHttpUrl(target)) {
            const Shortcut url_item = ShortcutFromUrl(target);
            item.command_line = command;
            item.keyword = url_item.keyword;
            item.name = url_item.name;
            return item;
        }
    }

    if (IsHttpUrl(command)) {
        return ShortcutFromUrl(command);
    }

    const std::string token = FirstToken(command);
    if (!token.empty() &&
        (EndsWithNoCase(token, ToUtf8(kLinkExtension)) ||
         FileOrDirectoryExists(token))) {
        // The first token names a real file/directory (or a .lnk): derive the
        // fields from it but keep the arguments in command_line.
        item = ShortcutFromPath(token);
        item.command_line = command;
        return item;
    }

    // No recognisable program token: treat the whole input as a path/URL.
    item = ShortcutFromPath(command);
    item.command_line = command;
    return item;
}

std::string ResolveLnk(const std::wstring& lnk_path) {
    if (lnk_path.empty()) {
        return {};
    }
    const std::wstring path = StripSurroundingQuotes(lnk_path);
    if (path.empty()) {
        return {};
    }

    IShellLinkW* link = nullptr;
    // Fails with CO_E_NOTINITIALIZED when COM was not initialized by the
    // caller; that is reported as "cannot resolve", not as a crash.
    HRESULT result = ::CoCreateInstance(CLSID_ShellLink, nullptr,
                                        CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&link));
    if (FAILED(result) || link == nullptr) {
        return {};
    }

    std::string resolved;
    IPersistFile* persist = nullptr;
    result = link->QueryInterface(IID_PPV_ARGS(&persist));
    if (SUCCEEDED(result) && persist != nullptr) {
        if (SUCCEEDED(persist->Load(path.c_str(), STGM_READ))) {
            wchar_t target[MAX_PATH * 2] = {};
            WIN32_FIND_DATAW find_data = {};
            const HRESULT path_result = link->GetPath(
                target, static_cast<int>(std::size(target)), &find_data, SLGP_RAWPATH);
            if (SUCCEEDED(path_result) && target[0] != L'\0') {
                resolved = "\"" + ToUtf8(target) + "\"";
                wchar_t arguments[1024] = {};
                if (SUCCEEDED(link->GetArguments(arguments,
                                                 static_cast<int>(std::size(arguments)))) &&
                    arguments[0] != L'\0') {
                    resolved += ' ';
                    resolved += ToUtf8(arguments);
                }
            }
        }
        persist->Release();
    }
    link->Release();
    return resolved;
}

std::string ToRelativePath(const std::string& absolute_utf8) {
    const std::string path = std::string(TrimAscii(absolute_utf8));
    if (path.empty()) {
        return path;
    }
    // Only drive-letter absolute paths are convertible; URLs, UNC paths,
    // environment variables and already-relative input stay untouched.
    const std::wstring wide = ToWide(path);
    if (wide.size() < 3 || wide[1] != L':' ||
        !((wide[0] >= L'A' && wide[0] <= L'Z') || (wide[0] >= L'a' && wide[0] <= L'z')) ||
        (wide[2] != L'\\' && wide[2] != L'/')) {
        return path;
    }

    std::wstring directory = ShortcutStore::ExeDirectory();
    while (!directory.empty() &&
           (directory.back() == L'\\' || directory.back() == L'/')) {
        directory.pop_back();
    }
    if (directory.empty() || !WideStartsWithNoCase(wide, directory)) {
        return path;
    }
    std::wstring rest = wide.substr(directory.size());
    if (rest.empty()) {
        return ".";
    }
    if (rest.front() != L'\\' && rest.front() != L'/') {
        return path;  // same prefix but a different directory (e.g. C:\app2)
    }
    rest.erase(0, 1);
    if (rest.empty()) {
        return ".";
    }
    return ".\\" + ToUtf8(rest);
}

std::string ToAbsolutePath(const std::string& path_utf8) {
    const std::string path = std::string(TrimAscii(path_utf8));
    if (path.empty()) {
        return path;
    }
    const std::string_view view(path);
    const bool relative =
        path == "." || path.rfind(".\\", 0) == 0 || path.rfind("./", 0) == 0 ||
        path.rfind("..\\", 0) == 0 || path.rfind("../", 0) == 0;
    if (!relative) {
        return path;
    }

    std::wstring result = ShortcutStore::ExeDirectory();
    const std::wstring source = ToWide(path);
    size_t start = 0;
    while (start <= source.size()) {
        const size_t next = source.find_first_of(L"\\/", start);
        const std::wstring part =
            source.substr(start, next == std::wstring::npos ? std::wstring::npos
                                                            : next - start);
        if (part == L"..") {
            while (!result.empty() &&
                   (result.back() == L'\\' || result.back() == L'/')) {
                result.pop_back();
            }
            const size_t slash = result.find_last_of(L"\\/");
            if (slash == std::wstring::npos) {
                result.clear();
            } else {
                result.resize(slash);
            }
        } else if (!part.empty() && part != L".") {
            if (!result.empty() && result.back() != L'\\' && result.back() != L'/') {
                result.push_back(L'\\');
            }
            result += part;
        }
        if (next == std::wstring::npos) {
            break;
        }
        start = next + 1;
    }
    return ToUtf8(result);
}

}  // namespace alt
