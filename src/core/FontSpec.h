#pragma once

#include <string>

namespace alt {

// Codec for the legacy font strings stored in [GUI] TitleFont / KeywordFont /
// ListFont.  See docs/spec/02-config-options.md §1.7 for the authoritative
// description (original untALTRunOption.pas FontToStr/StrToFont):
//
//     <family>|<style set>|<size pt>|<TColor>|<charset>
//     e.g. "SimSun|[]|12|65535|0"
//
//   * segment 1: face name (localized, e.g. 宋体)
//   * segment 2: Delphi TFont.Style set, e.g. "[]" or "[fsBold]".  The rewrite
//     only distinguishes regular (0) from bold (1); an empty set is 0.
//   * segment 3: size in points (12 = the original default)
//   * segment 4: Delphi TColor = $00BBGGRR (65535 = yellow, 255 = red,
//     8388608 = navy).  Kept for ini round-tripping; the launcher paints with
//     its own palette.
//   * segment 5: charset (0 = DEFAULT_CHARSET, 134 = GB2312).  Empty is 0.
//
// Parsing is intentionally tolerant: a missing segment keeps the default and a
// malformed number falls back to the default instead of raising (the original
// StrToFont threw EConvertError for a bad number, spec §7.5 defect #2).
struct FontSpec {
    std::wstring family = L"Segoe UI";
    int charset = 0;            // legacy segment 5; "[]" (segment 2) also yields 0
    float size = 12.0f;         // legacy segment 3, points
    unsigned color = 0xFFFFFF;  // legacy segment 4, $00BBGGRR (0xFFFFFF = yellow)
    int style = 0;              // legacy segment 2 reduced to 0 = regular, 1 = bold

    // Parses a UTF-8 font string (the Settings fields are UTF-8 internally).
    static FontSpec Parse(const std::string& utf8);

    // Always emits five segments in the legacy order.
    std::string Serialize() const;

    bool bold() const { return style != 0; }
};

}  // namespace alt
