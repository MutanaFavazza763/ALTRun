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

// Keys that exist only in the rewrite (no counterpart in the legacy .lang):
// short, fixed strings the C++ UI shows where the original had a random hint
// or a compound caption.  Everything the original also had comes from
// kOriginalBuiltins (below), which is generated from the legacy language files.
constexpr BuiltinText kRewriteBuiltins[] = {
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
    // Original: [Common] CanNotExecute ("无法运行 \"%s %s\"") needs the resolved
    // command; the rewrite shows a plain sentence in the same message box.
    {"resCannotExecute", L"Cannot execute the selected shortcut.",
     L"无法执行所选快捷项。"},
    // Tray captions: the rewrite keeps a shorter menu than the VCL one.
    {"resMenuShow", L"Open ALTRun(&O)", L"打开 ALTRun(&O)"},
    {"resMenuManager", L"ShortCut Manager(&M)", L"快捷项管理(&M)"},
    {"resMenuConfig", L"Settings(&S)", L"设置(&S)"},
    {"resTrayTip", L"ALTRun - Alt+R to open", L"ALTRun - Alt+R 打开"},
    // Parameter prompt (the legacy dialog was built from InputQuery strings).
    {"resParamTitle", L"ALTRun Parameter", L"ALTRun 参数"},
    {"resParamPrompt", L"   Parameter", L"   输入参数"},
    {"resParamHint", L"Enter OK    Esc Clear/Cancel    Auto-close when idle",
     L"Enter 确定    Esc 清空/取消    空闲自动关闭"},
    // About window labels (the legacy dialog drew these hard-coded).
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
    // Config window: widgets the legacy dialog did not have (the numeric keys
    // existed in the ini but had no editor, see docs/spec/02-config-options.md).
    {"resHideDelay", L"Hide delay (sec)", L"自动隐藏延时（秒）"},
    {"resParamHistoryLimit", L"Parameter history entries",
     L"参数历史记录条数"},
    {"resGeneralPageHint",
     L"Hint: a delay of 0 hides the launcher right after it pops up; a history "
     L"size of 0 turns the parameter history off.",
     L"提示：延时为 0 时主窗口弹出后立即隐藏；参数历史条数为 0 时关闭参数历史记录。"},
    {"resLastItemHotKey", L"Last item hotkey", L"最近一项热键"},
    {"resHotkeyCue", L"Click the box, then press the key combination; Delete clears",
     L"点击后按下组合键，Delete 清除"},
    {"resHotkeyHint1",
     L"Click a box and press the combination to capture it, e.g. Alt + R; "
     L"Delete clears it.",
     L"点击输入框后直接按下组合键即可捕获，例如 Alt + R；按 Delete 清除。"},
    {"resHotkeyHint2",
     L"Tab / Shift+Tab move focus while capturing; only Ctrl+Tab is stored as "
     L"a hotkey.",
     L"捕获时 Tab / Shift+Tab 用于切换焦点，Ctrl+Tab 才会被当作热键保存。"},
    {"resHotkeyHint3",
     L"A conflicting or unparsable hotkey may fail to register after saving; "
     L"watch the tray notification.",
     L"热键冲突或写法无法识别时，保存后可能注册失败，请留意托盘区域提示。"},
    {"resListFormatHint",
     L"The list format must contain exactly two placeholders: keyword and name.",
     L"列表格式必须且只能包含两个占位符（关键字、名称）。"},
    {"resAppearanceHint",
     L"Width, transparency and language changes may need an ALTRun restart to "
     L"take full effect.",
     L"修改宽度/透明度/语言后，部分界面需要重启 ALTRun 才能完全生效。"},
    {"resPageDebug", L"Debug/About", L"调试/关于"},
    {"resTraceEnable", L"Debug logging (TraceEnable)", L"启用调试日志（TraceEnable）"},
    {"resRelatedFiles", L"Related files (next to ALTRun.exe):",
     L"相关文件（与 ALTRun.exe 同目录）："},
    {"resParamHistoryFile", L"Parameter history: ", L"参数历史文件："},
    {"resShortcutListFile", L"Shortcut list: ", L"快捷项列表："},
    {"resFavoriteListFile", L"Favorites: ", L"收藏映射："},
    {"resTraceEnableHint",
     L"TraceEnable takes effect after restarting ALTRun; the log goes to "
     L"%LOCALAPPDATA%\\ALTRun\\altRun.log.",
     L"TraceEnable 修改后需重启 ALTRun 才会生效；日志写入 "
     L"%LOCALAPPDATA%\\ALTRun\\altRun.log。"},
    {"resLabelVersion", L"Version: ", L"版本："},
    // Shortcut manager: dialogs and buttons the legacy VCL form did not have.
    {"resKeyword", L"Keyword", L"关键字"},
    {"resMoveUp", L"Up", L"上移"},
    {"resMoveDown", L"Down", L"下移"},
    {"resExport", L"Export…", L"导出…"},
    {"resImport", L"Import…", L"导入…"},
    {"resManagerHint",
     L"Hint: drag a row to reorder; double-click to edit, Delete to remove, "
     L"Insert to add, F2 to edit.",
     L"提示：拖动行可调整顺序；双击行编辑，Delete 删除，Insert 新增，F2 编辑。"},
    {"resBlankRow", L"(blank separator)", L"（空白分隔行）"},
    {"resSaveListFailed",
     L"Cannot save ShortCutList.txt; check write access to the program folder.",
     L"保存 ShortCutList.txt 失败，请检查程序目录的写入权限。"},
    {"resShortcutExists", L"This shortcut already exists!", L"该快捷项已存在！"},
    {"resDeleteConfirm", L"Delete \"%s\"?", L"确定要删除快捷项「%s」吗？"},
    {"resTitleDelete", L"Delete", L"删除"},
    {"resTitleExport", L"Export", L"导出"},
    {"resTitleImport", L"Import", L"导入"},
    {"resMenuAdd", L"Add(&A)", L"新增(&A)"},
    {"resMenuEdit", L"Edit(&E)", L"编辑(&E)"},
    {"resMenuDelete", L"Delete(&D)", L"删除(&D)"},
    {"resExportPickTitle", L"Export the shortcut list",
     L"导出快捷项列表"},
    {"resExportSameFile",
     L"The target is the current ShortCutList.txt; nothing to export.",
     L"目标文件就是当前的 ShortCutList.txt，无需导出。"},
    {"resExportSaveFailed",
     L"Cannot save ShortCutList.txt, export aborted.",
     L"保存 ShortCutList.txt 失败，无法导出。"},
    {"resExportFailedCode", L"Export failed, error code: ", L"导出失败，错误码："},
    {"resExportDone", L"Exported to:", L"已导出到："},
    {"resImportPickTitle", L"Import a shortcut list (replaces the current one)",
     L"导入快捷项列表（将替换当前列表）"},
    {"resImportReadFailed",
     L"Cannot read the selected file (too large or unreadable).",
     L"无法读取所选文件（文件过大或读取失败）。"},
    {"resImportNothing", L"The selected file has no importable shortcut.",
     L"所选文件中没有可导入的快捷项。"},
    {"resImportConfirm", L"Importing replaces the current shortcut list.",
     L"导入将替换当前的快捷项列表。"},
    {"resImportCurrent", L"Current: ", L"当前："},
    {"resImportIncoming", L"Import: ", L"导入："},
    {"resImportRows", L" rows (", L" 行（快捷项 "},
    {"resImportItems", L" shortcuts)", L" 条）"},
    {"resImportContinue", L"Continue?", L"是否继续？"},
    // Shortcut edit dialog: field labels and radios (the legacy VCL form had
    // the same fields but its captions lived in the DFM, not in the .lang).
    {"resLabelKeyword", L"Keyword:", L"关键字："},
    {"resLabelName", L"Name:", L"显示名："},
    {"resLabelCommandLine", L"Command line:", L"命令行："},
    {"resLabelParamType", L"Param type:", L"参数模式："},
    {"resParamNone", L"No param", L"无参数"},
    {"resParamNoEncoding", L"Param, no encoding", L"参数无编码"},
    // Short captions for the fixed-width radios; the manager column shows the
    // full legacy ParamType_2/3 text instead.
    {"resParamUrlQuery", L"URL encoding", L"URL 编码"},
    {"resParamUtf8Query", L"UTF-8 encoding", L"UTF-8 编码"},
    {"resPickFile", L"Pick file…", L"选择文件…"},
    {"resPickFolder", L"Pick folder…", L"选择目录…"},
    {"resPickFileTitle", L"Pick a file", L"选择文件"},
    {"resEditHint",
     L"Picking a file or folder fills the command line; an empty keyword "
     L"derives the keyword and name from the path.",
     L"选择文件/目录会自动填入命令行；关键字为空时按路径规则推导关键字与显示名。"},
    {"resTitleAddShortcut", L"New Shortcut", L"新增快捷项"},
    {"resTitleEditShortcut", L"Edit Shortcut", L"编辑快捷项"},
    {"resKeywordEmpty",
     L"The keyword cannot be empty.\r\n\r\nFor a blank separator row, clear the "
     L"command line as well.",
     L"关键字不能为空。\r\n\r\n如确实要保留一个空白分隔行，请把命令行也清空。"},
    {"resKeywordPipe",
     L"The keyword cannot contain '|'; it would break the ShortCutList.txt "
     L"format.",
     L"关键字不能包含半角竖线（|），否则会破坏 ShortCutList.txt 格式。"},
    {"resNameSeparator",
     L"The name cannot contain ',' or '|'; it would break the file format.",
     L"显示名不能包含半角逗号（,）或竖线（|），否则会破坏存档格式。"},
    {"resNeedCommandLine", L"Please enter a command line first.", L"请先填写命令行。"},
    {"resTestFailed", L"Cannot run this command line:\r\n",
     L"无法执行该命令行：\r\n"},
    {"resTestErrorCode", L"\r\nError code: ", L"\r\n错误码："},
};

// All 224 keys of the legacy language files (8 sections, 中文 + English), so the
// fallback is complete even when no <Lang>.lang sits next to the executable.
// Regenerate with 08-tools/generate_builtin_table.py after changing the legacy
// language files; do not hand-edit the include.
#include "core/LocalizationBuiltin.inc"

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
    // Built-in keys keep the original spelling; lookups are case-insensitive,
    // matching INI/profile API semantics.
    for (const BuiltinText& entry : kRewriteBuiltins) {
        if (IEqualsAscii(lowered, entry.key)) {
            return chinese_ ? entry.zh : entry.en;
        }
    }
    for (const BuiltinText& entry : kOriginalBuiltins) {
        if (IEqualsAscii(lowered, entry.key)) {
            return chinese_ ? entry.zh : entry.en;
        }
    }
    // A bare legacy key ("ConfigList_0") resolves to its "Section.Key" entry;
    // the first hit wins, so call sites that care about the section (Caption in
    // two different forms) must pass the qualified name.
    if (lowered.find('.') == std::string::npos) {
        for (const BuiltinText& entry : kOriginalBuiltins) {
            const std::string name(entry.key);
            const size_t dot = name.find('.');
            if (dot != std::string::npos && dot + 1 < name.size() &&
                IEqualsAscii(lowered, name.substr(dot + 1))) {
                return chinese_ ? entry.zh : entry.en;
            }
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
