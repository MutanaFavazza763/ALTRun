// Unit tests for the UI-independent core of ALTRun.
//
// Deliberately dependency free (no gtest/catch): a tiny CHECK macro plus a
// process exit code. Build with the `altRunTests` target and run it manually or
// through CTest (see CMakeLists.txt).
//
// Covered here:
//   * Encoding   GBK <-> UTF-8 round trip, strict UTF-8 validation
//   * Formatter  ListFormat placeholders, number prefixes, CJK display width
//   * Matcher    wildcard -> regex translation, score formula, Sort() rank
//   * ShortcutStore  legacy ShortCutList.txt line parsing/serialisation
//   * ParamHistory   rank bump + smallest-rank eviction
//   * ShortcutSource path/URL derivation and relative path conversion
//   * Localization   every legacy/res* key resolves in en_us, zh_cn and zh_hk
//                    (224-key built-in table + res* table, no external files)

#include <windows.h>
#include <objbase.h>

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "core/Encoding.h"
#include "core/FontSpec.h"
#include "core/Formatter.h"
#include "core/Localization.h"
#include "core/Logger.h"
#include "core/Matcher.h"
#include "core/ParamHistory.h"
#include "core/Shortcut.h"
#include "core/ShortcutSource.h"
#include "core/ShortcutStore.h"
#include "core/Text.h"

// Generated list of every key the UI can request (legacy + res*).  See
// 08-tools/generate_test_loc_keys.py.
#include "LocalizationKeys.inc"

namespace {

int g_checks = 0;
int g_failures = 0;

void Fail(const char* expression, const char* file, int line,
          const std::string& detail) {
    ++g_failures;
    std::printf("FAIL %s:%d  CHECK(%s)%s\n", file, line, expression,
                detail.empty() ? "" : ("  " + detail).c_str());
}

void Check(bool condition, const char* expression, const char* file, int line) {
    ++g_checks;
    if (!condition) {
        Fail(expression, file, line, "");
    }
}

template <typename Actual, typename Expected>
void CheckEq(const Actual& actual, const Expected& expected,
             const char* expression, const char* file, int line) {
    ++g_checks;
    if (!(actual == expected)) {
        Fail(expression, file, line, "");
    }
}

void CheckStrEq(const std::string& actual, const std::string& expected,
                const char* expression, const char* file, int line) {
    ++g_checks;
    if (actual != expected) {
        Fail(expression, file, line,
             "  expected [" + expected + "] got [" + actual + "]");
    }
}

void CheckIntEq(long long actual, long long expected, const char* expression,
                const char* file, int line) {
    ++g_checks;
    if (actual != expected) {
        Fail(expression, file, line, "  expected " + std::to_string(expected) +
                                         " got " + std::to_string(actual));
    }
}

#define CHECK(cond) Check((cond), #cond, __FILE__, __LINE__)
#define CHECK_EQ(a, b) CheckEq((a), (b), #a " == " #b, __FILE__, __LINE__)
#define CHECK_STR(a, b) CheckStrEq((a), (b), #a " == " #b, __FILE__, __LINE__)
#define CHECK_INT(a, b) CheckIntEq((a), (b), #a " == " #b, __FILE__, __LINE__)

// ---------------------------------------------------------------------------
// Encoding
// ---------------------------------------------------------------------------
void TestEncoding() {
    // GBK bytes for "中文": D6 D0 CE C4.
    const std::string gbk("\xD6\xD0\xCE\xC4", 4);
    const std::string utf8 = alt::AnsiToUtf8(gbk);
    CHECK_STR(utf8, "\xE4\xB8\xAD\xE6\x96\x87");

    const std::string back = alt::Utf8ToAnsi(utf8);
    CHECK_STR(back, gbk);

    // ASCII passes through unchanged in both directions.
    CHECK_STR(alt::AnsiToUtf8("plain"), "plain");
    CHECK_STR(alt::Utf8ToAnsi("plain"), "plain");

    CHECK(alt::IsValidUtf8("plain"));
    CHECK(alt::IsValidUtf8(utf8));
    CHECK(!alt::IsValidUtf8(gbk));
    CHECK(!alt::IsValidUtf8("\xC3\x28"));  // invalid continuation byte
}

// ---------------------------------------------------------------------------
// Formatter
// ---------------------------------------------------------------------------
void TestFormatter() {
    CHECK_INT(alt::DisplayWidth("abc"), 3);
    CHECK_INT(alt::DisplayWidth("中文"), 4);       // two wide chars
    CHECK_INT(alt::DisplayWidth("中a"), 3);
    CHECK_INT(alt::DisplayWidth(""), 0);

    // Default format pads the keyword to 25 display columns, then "| name".
    const std::string line = alt::FormatListLine("%-25s| %s", "note", "记事本");
    CHECK_INT(alt::DisplayWidth(line.substr(0, line.find('|'))), 25);
    CHECK(line.find("note") == 0);
    CHECK_STR(line.substr(line.find('|')), "| 记事本");

    CHECK_STR(alt::FormatListLine("%s (%s)", "note", "记事本"), "note (记事本)");
    CHECK_STR(alt::FormatListLine("%s [%s]", "a", "b"), "a [b]");
    // Unknown conversion is preserved literally.
    CHECK_STR(alt::FormatListLine("%d", "x", "y"), "%d");

    // The launcher draws two pixel columns for the "|"-separator forms ...
    const alt::ListLineParts parts =
        alt::FormatListParts("%-25s| %s", "note", "记事本");
    CHECK(parts.two_columns);
    CHECK(parts.left.find("note") == 0);
    CHECK_STR(parts.separator, "| ");
    CHECK_STR(parts.right, "记事本");

    // ... but formats that put the name inline stay a single string.
    const alt::ListLineParts parens =
        alt::FormatListParts("%s (%s)", "note", "记事本");
    CHECK(!parens.two_columns);
    CHECK_STR(parens.left, "note (记事本)");
    CHECK(parens.right.empty());

    // Text after the second placeholder belongs to the right column.
    const alt::ListLineParts suffix =
        alt::FormatListParts("%-8s | %s!", "note", "name");
    CHECK(suffix.two_columns);
    CHECK_STR(suffix.right, "name!");

    // Number prefixes: 1..9,0 by default; * marks parameter items.
    CHECK_STR(alt::NumberPrefix(0, false, false), " 1|");
    CHECK_STR(alt::NumberPrefix(1, true, false), "*2|");
    CHECK_STR(alt::NumberPrefix(9, false, false), " 0|");
    CHECK_STR(alt::NumberPrefix(10, false, false), "  |");
    CHECK_STR(alt::NumberPrefix(9, false, true), " 9|");
    CHECK_STR(alt::NumberPrefix(0, false, true), " 0|");
}

// ---------------------------------------------------------------------------
// Matcher
// ---------------------------------------------------------------------------
alt::ShortcutEntry MakeItem(const std::string& keyword, int freq = 0,
                            alt::ParamType type = alt::ParamType::None,
                            const std::string& command = "cmd") {
    alt::ShortcutEntry entry;
    entry.item.keyword = keyword;
    entry.item.name = keyword + "-name";
    entry.item.freq = freq;
    entry.item.param_type = type;
    entry.item.command_line = command;
    return entry;
}

void TestMatcher() {
    CHECK_STR(alt::Matcher::WildcardToRegex("no*"), "no.*");
    CHECK_STR(alt::Matcher::WildcardToRegex("n?tepad"), "n.tepad");
    CHECK_STR(alt::Matcher::WildcardToRegex(".*x"), ".*x");  // existing .* kept
    CHECK_STR(alt::Matcher::WildcardToRegex("plain"), "plain");

    // score = 1024 + freq*4 - pos*128 - (keyword_len - query_len)*16
    CHECK_INT(alt::Matcher::Score(0, 1, 8, 4), 1024 - 128 - 4 * 16);
    CHECK_INT(alt::Matcher::Score(7, 1, 7, 4), 1024 + 28 - 128 - 3 * 16);
    CHECK_INT(alt::Matcher::Score(0, 3, 4, 4), 1024 - 3 * 128);

    // Sort() rank: freq*1000 - 4*(S1-'A') - 2*(S2-'A') - 1*(S3-'A') - len
    alt::ShortcutEntry computer = MakeItem("Computer", 100);
    CHECK_INT(alt::Matcher::SortRank(computer.item), 100000 - 8 - 28 - 12 - 8);

    std::vector<alt::ShortcutEntry> items;
    items.push_back(MakeItem("notepad", 7));
    items.push_back(MakeItem("mspaint", 0));
    items.push_back(MakeItem("cmd", 6, alt::ParamType::NoEncoding, "cmd /k %p"));

    alt::MatchQuery query;
    query.text = "note";
    query.regex_mode = true;
    query.match_anywhere = true;

    std::vector<alt::MatchResult> results = alt::Matcher().Match(items, query);
    CHECK_INT(static_cast<long long>(results.size()), 1);
    if (results.size() == 1) {
        CHECK_INT(static_cast<long long>(results[0].entry_index), 0);
        CHECK_INT(results[0].score, 1024 + 28 - 128 - 3 * 16);
    }

    // Case folding is ASCII only.
    query.text = "NOTE";
    CHECK_INT(static_cast<long long>(alt::Matcher().Match(items, query).size()), 1);

    // Wildcard search.
    query.text = "no*ad";
    CHECK_INT(static_cast<long long>(alt::Matcher().Match(items, query).size()), 1);

    // Prefix-only mode rejects a middle hit.
    query.text = "pad";
    query.match_anywhere = false;
    CHECK_INT(static_cast<long long>(alt::Matcher().Match(items, query).size()), 0);

    // Non-regex substring mode.
    query.text = "pad";
    query.regex_mode = false;
    query.match_anywhere = true;
    CHECK_INT(static_cast<long long>(alt::Matcher().Match(items, query).size()), 1);

    // A single space means "no results" in the original.
    query.text = " ";
    query.regex_mode = true;
    CHECK_INT(static_cast<long long>(alt::Matcher().Match(items, query).size()), 0);

    // Blank rows never match.
    std::vector<alt::ShortcutEntry> with_blank = items;
    alt::ShortcutEntry blank;
    blank.is_blank = true;
    with_blank.insert(with_blank.begin(), blank);
    query.text = "note";
    results = alt::Matcher().Match(with_blank, query);
    CHECK_INT(static_cast<long long>(results.size()), 1);
    if (results.size() == 1) {
        CHECK_INT(static_cast<long long>(results[0].entry_index), 1);
    }
}

// ---------------------------------------------------------------------------
// ShortCutList.txt parsing / serialisation
// ---------------------------------------------------------------------------
void TestShortcutStoreParsing() {
    alt::ShortcutEntry entry;

    // Modern pipe format, padded fields.
    const std::string line =
        "F8       |                    |ie                  |IE\xE6\xB5\x8F\xE8\xA7\x88\xE5\x99\xA8"
        "              |iexplore.exe";
    CHECK(alt::ShortcutStore::ParseLine(line, &entry));
    CHECK(!entry.is_blank);
    CHECK_INT(entry.item.freq, 8);
    CHECK_STR(entry.item.keyword, "ie");
    CHECK_STR(entry.item.command_line, "iexplore.exe");
    CHECK(entry.item.param_type == alt::ParamType::None);

    // Parameter type field.
    CHECK(alt::ShortcutStore::ParseLine(
        "F7|No_Encoding|cmd|Dos\xE7\xAA\x97\xE5\x8F\xA3|cmd /k %p", &entry));
    CHECK(entry.item.param_type == alt::ParamType::NoEncoding);
    CHECK_STR(entry.item.keyword, "cmd");
    CHECK_STR(entry.item.command_line, "cmd /k %p");

    // Blank line is a separator row.
    CHECK(alt::ShortcutStore::ParseLine("", &entry));
    CHECK(entry.is_blank);

    // Legacy comma format (no F prefix, no param type).
    CHECK(alt::ShortcutStore::ParseLine("ie,IE,iexplore.exe", &entry));
    CHECK(!entry.is_blank);
    CHECK_STR(entry.item.keyword, "ie");
    CHECK_STR(entry.item.name, "IE");
    CHECK_STR(entry.item.command_line, "iexplore.exe");

    // Command line may contain the separator.
    CHECK(alt::ShortcutStore::ParseLine("F0||x|X|cmd /c echo a|b", &entry));
    CHECK_STR(entry.item.command_line, "cmd /c echo a|b");

    // Hand-edited display forms: "keyword|name|command" and the common
    // "keyword||name|command" typo. The name must never leak into the command.
    CHECK(alt::ShortcutStore::ParseLine("notepad|Notepad|notepad.exe", &entry));
    CHECK_STR(entry.item.keyword, "notepad");
    CHECK_STR(entry.item.name, "Notepad");
    CHECK_STR(entry.item.command_line, "notepad.exe");

    CHECK(alt::ShortcutStore::ParseLine("notepad||Notepad|notepad.exe", &entry));
    CHECK_STR(entry.item.keyword, "notepad");
    CHECK_STR(entry.item.name, "Notepad");
    CHECK_STR(entry.item.command_line, "notepad.exe");

    // Non-parsable row (fewer than three fields) is dropped.
    CHECK(!alt::ShortcutStore::ParseLine("garbage", &entry));

    // Serialise -> parse round trip keeps every field.
    alt::ShortcutEntry original;
    original.item.freq = 12;
    original.item.param_type = alt::ParamType::UrlQuery;
    original.item.keyword = "b";
    original.item.name = "\xE7\x99\xBE\xE5\xBA\xA6";  // 百度
    original.item.command_line = "http://www.baidu.com/s?wd=";
    const std::string formatted = alt::ShortcutStore::FormatEntry(original);
    CHECK(formatted.rfind("F12", 0) == 0);
    alt::ShortcutEntry parsed;
    CHECK(alt::ShortcutStore::ParseLine(formatted, &parsed));
    CHECK_INT(parsed.item.freq, 12);
    CHECK(parsed.item.param_type == alt::ParamType::UrlQuery);
    CHECK_STR(parsed.item.keyword, original.item.keyword);
    CHECK_STR(parsed.item.name, original.item.name);
    CHECK_STR(parsed.item.command_line, original.item.command_line);

    // Blank entry serialises to an empty line.
    alt::ShortcutEntry blank;
    blank.is_blank = true;
    CHECK_STR(alt::ShortcutStore::FormatEntry(blank), "");
}

// ---------------------------------------------------------------------------
// ParamHistory (in-memory semantics only; file IO needs a writable exe dir)
// ---------------------------------------------------------------------------
void TestParamHistory() {
    alt::ParamHistory history;
    CHECK_INT(static_cast<long long>(history.items().size()), 0);

    history.Record("alpha", 50);
    CHECK_INT(static_cast<long long>(history.items().size()), 1);
    CHECK_INT(history.items()[0].rank, 0);

    // Existing value: rank + 1, position unchanged.
    history.Record("alpha", 50);
    CHECK_INT(static_cast<long long>(history.items().size()), 1);
    CHECK_INT(history.items()[0].rank, 1);

    // New value goes to the front with rank 0.
    history.Record("beta", 50);
    CHECK_INT(static_cast<long long>(history.items().size()), 2);
    CHECK_STR(history.items()[0].text, "beta");
    CHECK_STR(history.items()[1].text, "alpha");

    // Empty text is ignored.
    history.Record("   ", 50);
    CHECK_INT(static_cast<long long>(history.items().size()), 2);

    // Eviction: limit 2 and the *smallest* rank is removed. beta has rank 0
    // (the original documents that fresh entries are evicted first).
    history.Record("gamma", 2);
    CHECK_INT(static_cast<long long>(history.items().size()), 2);
    CHECK_STR(history.items()[0].text, "gamma");
    CHECK_STR(history.items()[1].text, "alpha");

    const std::wstring path = alt::ShortcutStore::ExeDirectory() +
        L"param-regression-" + std::to_wstring(::GetCurrentProcessId()) + L".txt";
    // Own this unique fixture only; never overwrite a user's history file.
    CHECK(::GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES);
    alt::ParamHistory saved(path);
    saved.Record("100%", 50);
    saved.Record("%PATH%", 50);
    saved.Record("a|b%", 50);
    CHECK(saved.Save(50));
    alt::ParamHistory loaded(path);
    CHECK(loaded.Load(50));
    CHECK_INT(loaded.items().size(), 3);
    if (loaded.items().size() == 3) {
        CHECK_STR(loaded.items()[0].text, "a|b%");
        CHECK_STR(loaded.items()[1].text, "%PATH%");
        CHECK_STR(loaded.items()[2].text, "100%");
    }
    CHECK(::DeleteFileW(path.c_str()) != FALSE);
}

// ---------------------------------------------------------------------------
// ShortcutSource (path / URL derivation)
// ---------------------------------------------------------------------------
void TestShortcutSource() {
    alt::Shortcut from_file = alt::ShortcutFromPath("C:\\Windows\\notepad.exe");
    CHECK_STR(from_file.keyword, "notepad");
    CHECK_STR(from_file.name, "notepad");
    CHECK_STR(from_file.command_line, "C:\\Windows\\notepad.exe");

    // Directories keep their full leaf name ("1.2图片" must not become "1").
    // The directory is created under the current working directory because
    // sandboxed environments may deny writes to %TEMP%.
    wchar_t cwd[MAX_PATH] = {};
    ::GetCurrentDirectoryW(MAX_PATH, cwd);
    const std::wstring dir =
        std::wstring(cwd) + L"\\altRunTest_1.2\x56FE\x7247";
    const BOOL created = ::CreateDirectoryW(dir.c_str(), nullptr);
    CHECK(created != FALSE || ::GetLastError() == ERROR_ALREADY_EXISTS);
    alt::Shortcut from_dir = alt::ShortcutFromPath(alt::ToUtf8(dir));
    CHECK_STR(from_dir.keyword, "altRunTest_1.2\xE5\x9B\xBE\xE7\x89\x87");
    ::RemoveDirectoryW(dir.c_str());

    alt::Shortcut from_url = alt::ShortcutFromUrl("http://www.baidu.com/s?wd=");
    CHECK_STR(from_url.keyword, "www.baidu.com");
    CHECK_STR(from_url.command_line, "http://www.baidu.com/s?wd=");
    CHECK(from_url.param_type == alt::ParamType::None);

    alt::Shortcut from_cmd =
        alt::ShortcutFromCommandLine("C:\\tools\\app.exe --flag");
    CHECK_STR(from_cmd.keyword, "app");

    // Missing .lnk resolves to an empty string instead of crashing.
    CHECK_STR(alt::ResolveLnk(L"C:\\definitely\\missing.lnk"), "");
}

// ---------------------------------------------------------------------------
// FontSpec (legacy [GUI] TitleFont/KeywordFont/ListFont strings)
// ---------------------------------------------------------------------------
void TestFontSpec() {
    alt::FontSpec spec = alt::FontSpec::Parse("SimSun|[]|12|65535|0");
    CHECK(spec.family == L"SimSun");
    CHECK_INT(spec.style, 0);
    CHECK(std::fabs(spec.size - 12.0f) < 0.01f);
    CHECK_INT(spec.color, 65535);
    CHECK_INT(spec.charset, 0);

    alt::FontSpec bold = alt::FontSpec::Parse("Consolas|[fsBold]|16|16777215|134");
    CHECK(bold.family == L"Consolas");
    CHECK_INT(bold.style, 1);
    CHECK(bold.bold());
    CHECK(std::fabs(bold.size - 16.0f) < 0.01f);
    CHECK_INT(bold.color, 16777215);
    CHECK_INT(bold.charset, 134);

    // Tolerant parsing: missing segments keep defaults, bad numbers fall back.
    alt::FontSpec partial = alt::FontSpec::Parse("Arial");
    CHECK(partial.family == L"Arial");
    CHECK(std::fabs(partial.size - 12.0f) < 0.01f);
    alt::FontSpec broken = alt::FontSpec::Parse("Arial|[]|abc|notacolor|x");
    CHECK(broken.family == L"Arial");
    CHECK(std::fabs(broken.size - 12.0f) < 0.01f);

    // Serialise -> parse round trip (always five segments).
    const std::string serialized = bold.Serialize();
    alt::FontSpec roundtrip = alt::FontSpec::Parse(serialized);
    CHECK(roundtrip.family == bold.family);
    CHECK_INT(roundtrip.style, bold.style);
    CHECK_INT(roundtrip.color, bold.color);
    CHECK_INT(roundtrip.charset, bold.charset);
    CHECK(std::fabs(roundtrip.size - bold.size) < 0.01f);
}

// ---------------------------------------------------------------------------
// Localization (three built-in locales; no external .lang/About.<Lang> files)
// ---------------------------------------------------------------------------
void TestLocalization() {
    alt::Localization& loc = alt::Localization::Instance();
    loc.Load("en_us");
    CHECK(!loc.Get("resTitle").empty());
    CHECK(!loc.Get("resHintInput").empty());
    // Unknown keys fall back to the key itself.
    CHECK(loc.Get("no.such.key") == L"no.such.key");
    CHECK(loc.Text("resNoMatch") == loc.Get("resNoMatch"));

    // Canonical values and migration from legacy ini spellings.
    CHECK(alt::Localization::NormalizeLanguage("en_us") == "en_us");
    CHECK(alt::Localization::NormalizeLanguage("EN") == "en_us");
    CHECK(alt::Localization::NormalizeLanguage("zh_cn") == "zh_cn");
    CHECK(alt::Localization::NormalizeLanguage("简体中文") == "zh_cn");
    CHECK(alt::Localization::NormalizeLanguage("zh-HK") == "zh_hk");
    CHECK(alt::Localization::NormalizeLanguage("繁體中文") == "zh_hk");
    CHECK(alt::Localization::NormalizeLanguage("de_DE") == "en_us");

    // Every UI key must resolve in all three built-in locales.  The list is
    // generated from the built-in tables by 08-tools/generate_test_loc_keys.py.
    const char* const kLanguages[] = {"en_us", "zh_cn", "zh_hk"};
    for (const char* language : kLanguages) {
        loc.Load(language);
        CHECK(loc.lang() == language);
        for (const char* key : kLocalizationKeys) {
            const std::wstring value = loc.Get(key);
            ++g_checks;
            // An empty value is legitimate (ConfigForm.VoidHotKey means "no
            // hotkey"); only a fall-through to the key itself is a failure.
            if (alt::ToUtf8(value) == key) {
                Fail(key, __FILE__, __LINE__, "built-in lookup fell through");
            }
        }
    }

    // The reload used by ApplySettings() must not lose the built-in table.
    loc.Load("en_us");
    CHECK(loc.Get("ConfigForm.ConfigList_0") == L"Auto run me when Windows start");
    loc.Load("zh_cn");
    CHECK(loc.Get("ConfigForm.ConfigList_0") == L"随系统自动启动");
    CHECK(loc.Get("DefaultShortCutName.Default_ShortCut_Name_3") == L"默认浏览器");
    CHECK(loc.Get("DefaultShortCutName.Default_ShortCut_Name_20") == L"Bing 搜索");
    CHECK(loc.Get("DefaultShortCutName.Default_ShortCut_Name_33") == L"Bing 搜索剪贴板");
    loc.Load("zh_hk");
    CHECK(loc.Get("ConfigForm.ConfigList_0") == L"隨系統自動啟動");
    CHECK(loc.Get("resSaveListFailed").find(L"儲存") != std::wstring::npos);
    CHECK(loc.Get("resImportReadFailed").find(L"檔案") != std::wstring::npos);
    loc.Load("en_us");
}

}  // namespace

int main() {
    // ShortcutSource uses COM (IShellLinkW); the application does the same in
    // main(), tests must initialise it as well.
    const HRESULT com = ::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    std::printf("ALTRun core tests\n");
    std::printf("-----------------\n");

    TestEncoding();
    TestFormatter();
    TestMatcher();
    TestShortcutStoreParsing();
    TestParamHistory();
    TestShortcutSource();
    TestFontSpec();
    TestLocalization();

    if (SUCCEEDED(com)) {
        ::CoUninitialize();
    }

    std::printf("-----------------\n%d checks, %d failure(s)\n", g_checks,
                g_failures);
    return g_failures == 0 ? 0 : 1;
}
