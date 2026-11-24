#pragma once

#include <string>
#include <unordered_map>

namespace alt {

// Language resources for the strings this rewrite actually displays.
//
// Legacy format (docs/spec/02-config-options.md §1.8/§6): <exe目录>\<Lang>.lang
// is an INI-styled file with [Section] lines and Key=Value entries, 224 keys in
// the original; keys are looked up here either bare ("MainHint") or fully
// qualified ("ALTRunForm.MainHint").  Files are usually ANSI/GBK but UTF-8
// (with or without BOM) and UTF-16 LE/BE are accepted.
//
// Lookup order in Get(): loaded .lang file -> built-in table for the language
// (EN = English, 简体中文/zh-* = Chinese) -> the key itself.  The built-in table
// only covers the keys used by the rewrite; key names follow the original
// res* variable naming style (原版键名见 docs/spec/02 §1.8 / §6.2).
class Localization {
public:
    static Localization& Instance();

    // Loads <exe目录>\<lang>.lang (lang is UTF-8, e.g. "EN" or "简体中文").
    // Returns true only when a file was found and parsed.  On false the
    // built-in table is used; Get() keeps working either way.
    bool Load(const std::string& lang);

    // file value -> built-in value -> key itself.
    std::wstring Get(const std::string& key) const;
    // Alias kept for call sites that read like UI text instead of resources.
    std::wstring Text(const std::string& key) const { return Get(key); }

    const std::string& lang() const { return lang_; }

    // Decodes a text file to UTF-8 (BOM aware, strict UTF-8 then legacy
    // ANSI/GBK).  Shared with AboutWindow for About.<Lang>.
    static bool ReadTextFileUtf8(const std::wstring& path, std::string* utf8);

private:
    Localization();

    static bool IsChineseLang(const std::string& lang);
    void Parse(const std::string& utf8);

    std::string lang_ = "EN";
    bool chinese_ = false;
    std::unordered_map<std::string, std::wstring> file_entries_;
};

}  // namespace alt
