#pragma once

// Hardcoded equivalent of submodules/twitter-text/config/v3.json, the default
// configuration used by TwitterTextParser's `defaultParser` on iOS and by
// TwitterTextConfiguration.getDefaultConfig() on Android. Keep this in sync if
// v3.json is ever updated upstream.

#include <icu.h>

#include <cstdint>

namespace TwitterText {

constexpr int64_t kConfigVersion = 3;
constexpr int64_t kConfigMaxWeightedTweetLength = 280;
constexpr int64_t kConfigScale = 100;
constexpr int64_t kConfigDefaultWeight = 200;
constexpr int64_t kConfigTransformedURLLength = 23;
constexpr bool kConfigEmojiParsingEnabled = true;

struct WeightedRange {
    UChar32 start;
    UChar32 end; // inclusive
    int64_t weight;
};

// Order matches config/v3.json's "ranges" array; the first matching range wins.
inline constexpr WeightedRange kConfigRanges[] = {
    {0, 4351, 100},
    {8192, 8205, 100},
    {8208, 8223, 100},
    {8242, 8247, 100},
};

inline int64_t WeightForCodePoint(UChar32 codePoint) {
    for (const WeightedRange& range : kConfigRanges) {
        if (codePoint >= range.start && codePoint <= range.end) {
            return range.weight;
        }
    }
    return kConfigDefaultWeight;
}

} // namespace TwitterText
