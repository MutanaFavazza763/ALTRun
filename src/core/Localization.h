#pragma once

#include <string>

namespace alt {

// Built-in language resources for the three supported UI locales.
//
// [GUI] Lang accepts exactly zh_cn, zh_hk and en_us.  Legacy values (EN,
// 简体中文, 繁體中文 and common zh-*/en spellings) are normalised on load so an
// existing ALTRun.ini keeps working; Save() writes the canonical lower-case
// value.  External <Lang>.lang files are deliberately no longer loaded.
//
// Lookup order in Get(): rewrite-only keys (res*) -> all 224 original keys ->
// bare-key suffix -> the key itself.  The 224-key table is generated from the
// legacy Simplified Chinese, Traditional Chinese and English files by
// 08-tools/generate_builtin_table.py into LocalizationBuiltin.inc.  Only the
// rewrite-only res* keys derive zh_hk at runtime, using Windows'
// LCMAP_TRADITIONAL_CHINESE plus UI terminology replacements.
class Localization {
public:
    static Localization& Instance();

    // Selects one of the three built-in locales.  Always returns true; unknown
    // values deliberately fall back to en_us.
    bool Load(const std::string& lang);

    // Converts aliases/legacy values to zh_cn, zh_hk or en_us.
    static std::string NormalizeLanguage(const std::string& lang);

    // Built-in value -> key itself.
    std::wstring Get(const std::string& key) const;
    // Alias kept for call sites that read like UI text instead of resources.
    std::wstring Text(const std::string& key) const { return Get(key); }

    const std::string& lang() const { return lang_; }

private:
    enum class Language { EnUs, ZhCn, ZhHk };

    Localization();

    std::wstring SelectText(const wchar_t* en_us, const wchar_t* zh_cn,
                            const wchar_t* zh_hk = nullptr) const;
    static std::wstring ToTraditionalChinese(std::wstring text);

    std::string lang_ = "en_us";
    Language language_ = Language::EnUs;
};

}  // namespace alt
