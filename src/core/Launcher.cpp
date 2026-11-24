#include "core/Launcher.h"

#include <windows.h>
#include <shellapi.h>

#include <string>
#include <vector>

#include "core/Encoding.h"
#include "core/Logger.h"
#include "core/ShortcutStore.h"
#include "core/Text.h"

namespace alt {
namespace {

std::wstring ExpandEnvironment(const std::wstring& text) {
    if (text.empty()) {
        return text;
    }
    const DWORD needed = ::ExpandEnvironmentStringsW(text.c_str(), nullptr, 0);
    if (needed == 0) {
        return text;
    }
    std::wstring out(needed, L'\0');
    const DWORD written =
        ::ExpandEnvironmentStringsW(text.c_str(), out.data(), needed);
    if (written == 0) {
        return text;
    }
    out.resize(written > 0 ? written - 1 : 0);  // drop trailing NUL
    return out;
}

void ReplaceAllW(std::wstring& text, const std::wstring& from,
                 const std::wstring& to) {
    if (from.empty()) {
        return;
    }
    size_t pos = 0;
    while ((pos = text.find(from, pos)) != std::wstring::npos) {
        text.replace(pos, from.size(), to);
        pos += to.size();
    }
}

bool FileExistsW(const std::wstring& path) {
    const DWORD attrs = ::GetFileAttributesW(path.c_str());
    return attrs != INVALID_FILE_ATTRIBUTES &&
           (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

bool DirectoryExistsW(const std::wstring& path) {
    const DWORD attrs = ::GetFileAttributesW(path.c_str());
    return attrs != INVALID_FILE_ATTRIBUTES &&
           (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

std::wstring DirectoryOf(const std::wstring& path) {
    const size_t slash = path.find_last_of(L"\\/");
    return slash == std::wstring::npos ? std::wstring() : path.substr(0, slash);
}

std::wstring ForegroundWindowTitle() {
    const HWND foreground = ::GetForegroundWindow();
    if (foreground == nullptr) {
        return {};
    }
    wchar_t title[512] = {};
    ::GetWindowTextW(foreground, title, static_cast<int>(std::size(title)));
    return title;
}

std::wstring ForegroundWindowClass() {
    const HWND foreground = ::GetForegroundWindow();
    if (foreground == nullptr) {
        return {};
    }
    wchar_t klass[256] = {};
    ::GetClassNameW(foreground, klass, static_cast<int>(std::size(klass)));
    return klass;
}

std::wstring ForegroundWindowId() {
    const HWND foreground = ::GetForegroundWindow();
    return std::to_wstring(reinterpret_cast<unsigned long long>(foreground));
}

// Applies the @ / @+ / @- display prefix (spec §8.6) and reports the show mode.
int StripShowPrefix(std::wstring* command_line) {
    int show = SW_SHOWNORMAL;
    if (command_line->rfind(L"@+", 0) == 0) {
        show = SW_SHOWMAXIMIZED;
        command_line->erase(0, 2);
    } else if (command_line->rfind(L"@-", 0) == 0) {
        show = SW_SHOWMINIMIZED;
        command_line->erase(0, 2);
    } else if (command_line->rfind(L"@", 0) == 0) {
        show = SW_HIDE;
        command_line->erase(0, 1);
    }
    return show;
}

// Removes one pair of enclosing quotes (Delphi RemoveQuotationMark).
std::wstring RemoveQuotationMark(std::wstring text) {
    if (text.size() >= 2 && text.front() == L'"' && text.back() == L'"') {
        return text.substr(1, text.size() - 2);
    }
    return text;
}

std::wstring TrimW(std::wstring text) {
    size_t begin = 0;
    size_t end = text.size();
    while (begin < end && text[begin] <= L' ') ++begin;
    while (end > begin && text[end - 1] <= L' ') --end;
    text = text.substr(begin, end - begin);
    return text;
}

}  // namespace

std::string PercentEncode(std::string_view bytes) {
    static const char* hex = "0123456789ABCDEF";
    std::string out;
    out.reserve(bytes.size() * 3);
    for (unsigned char c : bytes) {
        // Unreserved characters stay as-is (URL encoding convention).
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' ||
            c == '~') {
            out.push_back(static_cast<char>(c));
        } else {
            out.push_back('%');
            out.push_back(hex[c >> 4]);
            out.push_back(hex[c & 0x0F]);
        }
    }
    return out;
}

std::string EncodeParam(ParamType type, std::string_view param) {
    switch (type) {
        case ParamType::None:
        case ParamType::NoEncoding:
            return std::string(param);
        case ParamType::UrlQuery:
            return PercentEncode(Utf8ToAnsi(param));  // ANSI/GBK bytes
        case ParamType::Utf8Query:
            return PercentEncode(param);  // UTF-8 bytes
    }
    return std::string(param);
}

std::string ClipboardText() {
    if (!::OpenClipboard(nullptr)) {
        return {};
    }
    std::string result;
    HANDLE data = ::GetClipboardData(CF_UNICODETEXT);
    if (data != nullptr) {
        const auto* text = static_cast<const wchar_t*>(::GlobalLock(data));
        if (text != nullptr) {
            result = ToUtf8(text);
            ::GlobalUnlock(data);
        }
    }
    ::CloseClipboard();
    return result;
}

bool HasParamPlaceholder(std::string_view command_line) {
    return command_line.find("{%p}") != std::string_view::npos ||
           command_line.find("%p") != std::string_view::npos;
}

bool NeedsParamPrompt(const Shortcut& item) {
    if (item.param_type == ParamType::None) {
        return false;
    }
    // {%c}/{%wd}/{%wt}/{%wc} provide their own value, so no prompt is needed.
    const bool has_auto_value =
        item.command_line.find("{%c}") != std::string::npos ||
        item.command_line.find("{%wd}") != std::string::npos ||
        item.command_line.find("{%wt}") != std::string::npos ||
        item.command_line.find("{%wc}") != std::string::npos;
    if (has_auto_value) {
        return false;
    }
    // {%p}/%p need user input, and URL/UTF8 query types append the parameter.
    return true;
}

LaunchPlan BuildLaunchPlan(const Shortcut& item, std::string_view param) {
    LaunchPlan plan;

    std::wstring command_line = ExpandEnvironment(ToWide(item.command_line));

    // 1. Substitute the highest-priority placeholder (only one is replaced,
    //    mirroring the original short-circuit order).
    const std::string encoded = EncodeParam(item.param_type, param);
    const std::wstring encoded_wide = ToWide(encoded);
    bool substituted = false;
    const struct {
        const wchar_t* token;
        bool needs_value;
    } placeholders[] = {
        {L"{%p}", true}, {L"%p", true}, {L"{%c}", false},
        {L"{%wd}", false}, {L"{%wt}", false}, {L"{%wc}", false},
    };
    for (const auto& placeholder : placeholders) {
        if (command_line.find(placeholder.token) == std::wstring::npos) {
            continue;
        }
        std::wstring value = encoded_wide;
        if (std::wstring(placeholder.token) == L"{%c}") {
            value = ToWide(ClipboardText());
        } else if (std::wstring(placeholder.token) == L"{%wd}") {
            value = ForegroundWindowId();
        } else if (std::wstring(placeholder.token) == L"{%wt}") {
            value = ForegroundWindowTitle();
        } else if (std::wstring(placeholder.token) == L"{%wc}") {
            value = ForegroundWindowClass();
        }
        ReplaceAllW(command_line, placeholder.token, value);
        substituted = true;
        break;
    }

    // 2. No placeholder: URL_Query/UTF8_Query append the encoding, everything
    //    else appends the raw parameter.
    if (!substituted && !encoded.empty()) {
        if (item.param_type == ParamType::UrlQuery ||
            item.param_type == ParamType::Utf8Query) {
            command_line += encoded_wide;
        } else {
            command_line += L" ";
            command_line += encoded_wide;
        }
    }

    // 3. Display prefix, then environment expansion again (like ExecuteCmd).
    plan.show_command = StripShowPrefix(&command_line);
    command_line = TrimW(ExpandEnvironment(command_line));
    plan.raw_command_line = command_line;

    // 4. Split "command + arguments" following the original rules (§8.4).
    const std::wstring unquoted = RemoveQuotationMark(command_line);
    if (FileExistsW(unquoted)) {
        plan.command = unquoted;
        plan.working_dir = DirectoryOf(unquoted);
        return plan;
    }

    if (!command_line.empty() && command_line.front() == L'"') {
        const size_t closing = command_line.find(L'"', 1);
        if (closing != std::wstring::npos) {
            const std::wstring first = command_line.substr(1, closing - 1);
            if (FileExistsW(first)) {
                plan.command = first;
                plan.parameters = TrimW(command_line.substr(closing + 1));
                plan.working_dir = DirectoryOf(first);
                return plan;
            }
        }
    }

    plan.command = command_line;

    // 5. Relative commands run from the executable directory (spec §8.3).
    const std::wstring trimmed = TrimW(command_line);
    if (trimmed.rfind(L".\\", 0) == 0 || trimmed.rfind(L"..\\", 0) == 0) {
        plan.working_dir = ShortcutStore::ExeDirectory();
    }
    return plan;
}

LaunchResult ExecutePlan(const LaunchPlan& plan) {
    LaunchResult result;
    if (plan.command.empty()) {
        result.message = "empty command";
        return result;
    }

    SHELLEXECUTEINFOW info = {};
    info.cbSize = sizeof(info);
    info.fMask = SEE_MASK_NOASYNC | SEE_MASK_FLAG_NO_UI | SEE_MASK_NOCLOSEPROCESS;
    info.lpVerb = L"open";
    info.lpFile = plan.command.c_str();
    info.lpParameters = plan.parameters.empty() ? nullptr : plan.parameters.c_str();
    info.lpDirectory = plan.working_dir.empty() ? nullptr : plan.working_dir.c_str();
    info.nShow = plan.show_command;

    if (::ShellExecuteExW(&info)) {
        if (info.hProcess != nullptr) {
            ::CloseHandle(info.hProcess);
        }
        result.ok = true;
        return result;
    }
    const DWORD shell_error = ::GetLastError();

    // Original fallback: WinExec on the whole command line.
    std::wstring fallback = plan.raw_command_line;
    if (!plan.parameters.empty() &&
        fallback.find(plan.parameters) == std::wstring::npos) {
        fallback += L" ";
        fallback += plan.parameters;
    }
    const std::string ansi = Utf8ToAnsi(ToUtf8(fallback));
    if (!ansi.empty()) {
        const UINT code = ::WinExec(ansi.c_str(), plan.show_command);
        if (code > 31) {
            result.ok = true;
            Logger::Instance().Info("launched via WinExec fallback");
            return result;
        }
    }

    result.error = shell_error;
    result.message = "ShellExecuteEx/WinExec failed: " +
                     std::to_string(static_cast<unsigned long>(shell_error));
    return result;
}

LaunchResult LaunchShortcut(const Shortcut& item, std::string_view param) {
    const LaunchPlan plan = BuildLaunchPlan(item, param);
    const LaunchResult result = ExecutePlan(plan);
    if (!result.ok) {
        Logger::Instance().Error("launch failed for [" + item.keyword + "] " +
                                 item.command_line + ": " + result.message);
    } else {
        Logger::Instance().Info("launched [" + item.keyword + "] " +
                                item.command_line);
    }
    return result;
}

}  // namespace alt
