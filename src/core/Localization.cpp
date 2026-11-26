#include "core/Localization.h"

#include <windows.h>

#include <cstddef>
#include <string>

#include "core/Logger.h"
#include "core/Text.h"

namespace alt {
namespace {

struct BuiltinText {
    const char* key;
    const wchar_t* en;
    const wchar_t* zh;
    const wchar_t* zh_hk = nullptr;
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
    {"resFluentSettingsSearch", L"Search & matching", L"搜索与匹配",
     L"搜尋與匹配"},
    {"resFluentSettingsGeneral", L"General", L"常规", L"一般"},
    {"resFluentSettingsStartup", L"Startup & display", L"启动与显示",
     L"啟動與顯示"},
    {"resFluentSettingsAdvanced", L"Advanced", L"高级选项", L"進階選項"},
    {"resFluentSettingsHotkeys", L"Global hotkeys", L"全局热键",
     L"全域熱鍵"},
    {"resFluentSettingsAppearance", L"Appearance", L"外观", L"外觀"},
    {"resAppearanceCardTitle", L"Window", L"窗口", L"視窗"},
    {"resFluentSettingsDisplay", L"List display", L"列表显示", L"清單顯示"},
    {"resFluentSettingsOptions", L"Window options", L"窗口选项",
     L"視窗選項"},
    {"resFluentSettingsAbout", L"Version & paths", L"版本与路径",
     L"版本與路徑"},
    {"resGeneralPageHint",
     L"Match behaviour, startup options and advanced limits.",
     L"调整搜索匹配、启动行为与高级限制。"},
    {"resSearchPageHint",
     L"Controls keyword matching, list density and what happens after a shortcut runs.",
     L"控制关键字匹配、列表密度与执行快捷项后的行为。"},
    {"resHideDelayHint",
     L"0 hides the launcher immediately after it appears.",
     L"设为 0 时，主窗口弹出后立即隐藏。"},
    {"resParamHistoryLimitHint",
     L"0 turns parameter history off.",
     L"设为 0 时关闭参数历史记录。"},
    {"resAboutPageHint",
     L"Build information, data files and diagnostic logging.",
     L"构建信息、数据文件与诊断日志。"},
    {"resAboutCardTitle", L"Application", L"应用信息", L"應用資訊"},
    {"resHotkeyPageHint",
     L"Click a box and press the key combination.",
     L"点击输入框后按下组合键即可捕获。"},
    {"resHideDelay", L"Hide delay (sec)", L"自动隐藏延时（秒）"},
    {"resParamHistoryLimit", L"Parameter history entries",
     L"参数历史记录条数"},
    {"resListFormat", L"List format", L"列表格式", L"清單格式"},
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
    {"resTheme", L"App theme", L"应用主题", L"應用主題"},
    {"resThemeSystem", L"Follow Windows", L"跟随 Windows", L"跟隨 Windows"},
    {"resThemeLight", L"Light", L"浅色", L"淺色"},
    {"resThemeDark", L"Dark", L"深色", L"深色"},
    {"resSkinFile", L"Launcher skin file", L"启动器皮肤文件", L"啟動器皮膚檔案"},
    {"resPickSkin", L"Choose image…", L"选择图片…", L"選擇圖片…"},
    {"resSkinHint", L"Choose a JPG, PNG or BMP next to ALTRun.exe; the launcher will use it when skin is enabled.",
     L"选择与 ALTRun.exe 同目录的 JPG、PNG 或 BMP；开启皮肤后启动器会使用它。",
     L"選擇與 ALTRun.exe 同目錄的 JPG、PNG 或 BMP；開啟皮膚後啟動器會使用它。"},
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
    {"resCopyPath", L"Copy path", L"复制路径", L"複製路徑"},
    {"resOpenPath", L"Open path", L"打开路径", L"開啟路徑"},
    {"resAdvancedHint", L"Changes take effect after you click Save.",
     L"修改后点击“确定”保存生效。", L"修改後點擊「確定」儲存生效。"},
    // Shortcut manager: dialogs and buttons the legacy VCL form did not have.
    {"resKeyword", L"Keyword", L"关键字"},
    {"resMoveUp", L"Up", L"上移"},
    {"resMoveDown", L"Down", L"下移"},
    {"resExport", L"Export…", L"导出…"},
    {"resImport", L"Import…", L"导入…"},
    {"resManagerHint",
     L"Hint: drag a row to reorder; double-click to edit, Delete to remove, "
     L"Insert to add, F2 to edit. Hover a cell for its full text.",
     L"提示：拖动行可调整顺序；双击行编辑，Delete 删除，Insert 新增，F2 编辑。悬停单元格可查看完整内容。"},
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
    // First-run welcome window (original: language picker + AutoRun/SendTo
    // questions, plus the "use the hotkey" hint; the rewrite adds a short
    // usage guide because the legacy dialogs never explained the list).
    {"resWelcomeTitle", L"Welcome to ALTRun", L"欢迎使用 ALTRun",
     L"歡迎使用 ALTRun"},
    {"resWelcomeIntro",
     L"Two quick choices, then the basics.",
     L"先做两个选择，再看一眼基本用法。",
     L"先做兩個選擇，再看一眼基本用法。"},
    {"resWelcomeGuide",
     L"Press [[%s]] to show or hide the launcher.\r\n"
     L"Type a keyword and press [[Enter]] to run the selected shortcut.\r\n"
     L"Shortcuts marked * need a parameter; press [[Enter]] in the prompt.\r\n"
     L"Right-click the tray icon for Settings, Shortcut Manager and Exit.",
     L"按 [[%s]] 随时呼出或隐藏启动器。\r\n"
     L"输入关键字后按 [[Enter]] 运行选中的快捷项。\r\n"
     L"带有 * 标记的快捷项需要参数，弹出窗口按 [[Enter]] 执行。\r\n"
     L"右键托盘图标可打开设置、快捷项管理和退出。",
     L"按 [[%s]] 隨時呼出或隱藏啟動器。\r\n"
     L"輸入關鍵字後按 [[Enter]] 執行選取的捷徑。\r\n"
     L"帶有 * 標記的捷徑項目需要參數，彈出視窗按 [[Enter]] 執行。\r\n"
     L"右鍵點擊系統匣圖示可開啟設定、捷徑管理與結束。"},
    {"resWelcomeLanguage", L"Language", L"界面语言", L"介面語言"},
    {"resWelcomeAutoRun", L"Start ALTRun with Windows", L"随 Windows 启动",
     L"隨 Windows 啟動"},
    {"resWelcomeSendTo", L"Add ALTRun to the Send To menu",
     L"添加到“发送到”菜单", L"加入「傳送到」功能表"},
    {"resWelcomeNote",
     L"You can change the language and both options later in Settings.",
     L"语言和这两个选项以后都能在“设置”里修改。",
     L"語言與這兩個選項之後都能在「設定」中修改。"},
    {"resWelcomeStart", L"Start using ALTRun", L"开始使用", L"開始使用"},
};

// All 224 keys of the legacy language files (8 sections, en_us / zh_cn / zh_hk).
// Regenerate with 08-tools/generate_builtin_table.py after changing the legacy
// source tables; do not hand-edit the include.
#include "core/LocalizationBuiltin.inc"

void ReplaceAll(std::wstring* text, const wchar_t* from, const wchar_t* to) {
    const std::wstring source(from);
    const std::wstring target(to);
    size_t at = 0;
    while ((at = text->find(source, at)) != std::wstring::npos) {
        text->replace(at, source.size(), target);
        at += target.size();
    }
}

}  // namespace

Localization& Localization::Instance() {
    static Localization instance;
    return instance;
}

Localization::Localization() = default;

bool Localization::Load(const std::string& lang) {
    lang_ = NormalizeLanguage(lang);
    language_ = lang_ == "zh_cn" ? Language::ZhCn
                                 : (lang_ == "zh_hk" ? Language::ZhHk
                                                     : Language::EnUs);
    Logger::Instance().Info("localization: using built-in " + lang_ + " strings");
    return true;
}

std::string Localization::NormalizeLanguage(const std::string& lang) {
    std::string lowered = ToLowerUtf8(TrimAscii(lang));
    for (char& character : lowered) {
        if (character == '-') {
            character = '_';
        }
    }
    if (lowered == "zh_hk" || lowered == "zh_tw" || lowered == "zh_hant" ||
        lang.find("繁体") != std::string::npos ||
        lang.find("繁體") != std::string::npos) {
        return "zh_hk";
    }
    if (lowered == "zh" || lowered == "zh_cn" || lowered == "zh_hans" ||
        lang.find("简体") != std::string::npos ||
        lang.find("簡體") != std::string::npos) {
        return "zh_cn";
    }
    return "en_us";
}

std::string Localization::SystemDefaultLanguage() {
    const LANGID id = ::GetUserDefaultUILanguage();
    if (PRIMARYLANGID(id) == LANG_CHINESE) {
        switch (SUBLANGID(id)) {
            case SUBLANG_CHINESE_TRADITIONAL:
            case SUBLANG_CHINESE_HONGKONG:
            case SUBLANG_CHINESE_MACAU:
                return "zh_hk";
            default:
                return "zh_cn";
        }
    }
    return "en_us";
}

std::wstring Localization::Get(const std::string& key) const {
    const std::string lowered = ToLowerUtf8(key);
    // Built-in keys keep the original spelling; lookups are case-insensitive,
    // matching INI/profile API semantics.
    for (const BuiltinText& entry : kRewriteBuiltins) {
        if (IEqualsAscii(lowered, entry.key)) {
            return SelectText(entry.en, entry.zh);
        }
    }
    for (const BuiltinText& entry : kOriginalBuiltins) {
        if (IEqualsAscii(lowered, entry.key)) {
            return SelectText(entry.en, entry.zh, entry.zh_hk);
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
                return SelectText(entry.en, entry.zh, entry.zh_hk);
            }
        }
    }
    return ToWide(key);
}

std::wstring Localization::SelectText(const wchar_t* en_us,
                                      const wchar_t* zh_cn,
                                      const wchar_t* zh_hk) const {
    if (language_ == Language::EnUs) {
        return en_us;
    }
    if (language_ == Language::ZhCn) {
        return zh_cn;
    }
    // A few entries in the legacy Traditional file were left in Simplified
    // Chinese (e.g. ConfigList_0); normalise glyphs even for native zh_hk text.
    return ToTraditionalChinese(zh_hk != nullptr ? zh_hk : zh_cn);
}

std::wstring Localization::ToTraditionalChinese(std::wstring text) {
    if (text.empty()) {
        return text;
    }

    // LCMapStringEx converts characters, not vocabulary.  Apply the common
    // Windows UI terms first so zh_hk reads like a Traditional Chinese desktop
    // application rather than a glyph-only conversion of the Simplified text.
    // Longer/more specific phrases must precede their substrings.
    static constexpr struct {
        const wchar_t* simplified;
        const wchar_t* traditional;
    } kTerms[] = {
        {L"快捷方式", L"捷徑"},
        {L"快捷项", L"捷徑"},
        {L"配置文件", L"設定檔"},
        {L"文件夹", L"資料夾"},
        {L"文件", L"檔案"},
        {L"命令行", L"命令列"},
        {L"显示名", L"顯示名稱"},
        {L"默认", L"預設"},
        {L"设置", L"設定"},
        {L"保存", L"儲存"},
        {L"导入", L"匯入"},
        {L"导出", L"匯出"},
        {L"运行", L"執行"},
        {L"窗口", L"視窗"},
        {L"菜单", L"選單"},
        {L"字体", L"字型"},
        {L"数据", L"資料"},
        {L"程序", L"程式"},
        {L"信息", L"資訊"},
        {L"用户", L"使用者"},
        {L"支持", L"支援"},
        {L"字符", L"字元"},
        {L"热键", L"快速鍵"},
        {L"粘贴", L"貼上"},
        {L"链接", L"連結"},
        {L"列表", L"清單"},
        {L"添加", L"新增"},
        {L"创建", L"建立"},
    };
    for (const auto& term : kTerms) {
        ReplaceAll(&text, term.simplified, term.traditional);
    }

    const int source_length = static_cast<int>(text.size());
    const int required = ::LCMapStringEx(
        L"zh-CN", LCMAP_TRADITIONAL_CHINESE, text.data(), source_length, nullptr,
        0, nullptr, nullptr, 0);
    if (required <= 0) {
        return text;
    }
    std::wstring converted(static_cast<size_t>(required), L'\0');
    const int written = ::LCMapStringEx(
        L"zh-CN", LCMAP_TRADITIONAL_CHINESE, text.data(), source_length,
        converted.data(), required, nullptr, nullptr, 0);
    if (written != required) {
        return text;
    }
    return converted;
}

}  // namespace alt
