#pragma once

// C++/ICU port of the twitter-text parsing algorithm (see
// submodules/twitter-text/objc/lib/TwitterText.{h,m} and
// submodules/twitter-text/objc/lib/TwitterTextEmoji.{h,m}), ported so that the
// Windows Turbo Module does not have to fall back to the pure-JS implementation.
//
// This file intentionally mirrors the structure of the Objective-C
// implementation method-for-method so that the two can be diffed against each
// other when twitter-text is updated upstream.

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace TwitterText {

enum class EntityType {
    Url,
    ScreenName,
    Hashtag,
    ListName,
    Symbol,
};

struct Entity {
    EntityType type;
    int32_t start; // UTF-16 code unit offset, inclusive
    int32_t end;   // UTF-16 code unit offset, exclusive
};

struct ParseResults {
    int64_t weightedLength;
    int64_t permillage;
    bool isValid;
    int32_t displayTextRangeStart;
    int32_t displayTextRangeEnd;
    int32_t validTextRangeStart;
    int32_t validTextRangeEnd;
};

// Text is always UTF-16 (matching NSString/Java String semantics, and ICU's
// own UChar representation) so that entity start/end offsets line up exactly
// with the UTF-16 code unit indices React Native's JS strings use.
std::vector<Entity> EntitiesInText(const std::u16string& text);
std::vector<Entity> UrlsInText(const std::u16string& text);
std::vector<Entity> HashtagsInText(const std::u16string& text, bool checkingUrlOverlap);
std::vector<Entity> SymbolsInText(const std::u16string& text, bool checkingUrlOverlap);
std::vector<Entity> MentionedScreenNamesInText(const std::u16string& text);
std::vector<Entity> MentionsOrListsInText(const std::u16string& text);
std::optional<Entity> RepliedScreenNameInText(const std::u16string& text);

// For TwitterTextEntityListName entities, the list slug (including the
// leading '/') is everything in the matched value after the first '/'.
std::optional<std::u16string> ListSlugForEntity(const std::u16string& text, const Entity& entity);

bool IsValidHashtagText(const std::u16string& text);

int64_t TweetLength(const std::u16string& text, int64_t transformedUrlLength = 23);

ParseResults ParseTweet(const std::u16string& text);

} // namespace TwitterText
