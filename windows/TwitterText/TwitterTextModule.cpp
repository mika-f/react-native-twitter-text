#include "pch.h"

#include "TwitterTextModule.h"
#include "TwitterTextCore.h"

#include <icu.h>

#include <algorithm>

namespace winrt::TwitterText {

namespace {

// Windows only exposes ICU's C API (see the comment at the top of
// TwitterTextCore.cpp) so UTF-8 <-> UTF-16 conversion at the JS boundary goes
// through u_strFromUTF8 / u_strToUTF8 rather than icu::UnicodeString.

std::u16string Utf16FromUtf8(const std::string& utf8) {
    UErrorCode status = U_ZERO_ERROR;
    int32_t requiredLength = 0;
    u_strFromUTF8(nullptr, 0, &requiredLength, utf8.data(), static_cast<int32_t>(utf8.length()), &status);
    if (status != U_BUFFER_OVERFLOW_ERROR && U_FAILURE(status)) {
        return std::u16string();
    }
    status = U_ZERO_ERROR;
    std::u16string result(static_cast<size_t>(requiredLength), u'\0');
    u_strFromUTF8(reinterpret_cast<UChar*>(result.data()), requiredLength, nullptr, utf8.data(), static_cast<int32_t>(utf8.length()), &status);
    if (U_FAILURE(status)) {
        return std::u16string();
    }
    return result;
}

std::string Utf8FromUtf16(const std::u16string& utf16) {
    UErrorCode status = U_ZERO_ERROR;
    int32_t requiredLength = 0;
    u_strToUTF8(nullptr, 0, &requiredLength, reinterpret_cast<const UChar*>(utf16.data()), static_cast<int32_t>(utf16.length()), &status);
    if (status != U_BUFFER_OVERFLOW_ERROR && U_FAILURE(status)) {
        return std::string();
    }
    status = U_ZERO_ERROR;
    std::string result(static_cast<size_t>(requiredLength), '\0');
    u_strToUTF8(result.data(), requiredLength, nullptr, reinterpret_cast<const UChar*>(utf16.data()), static_cast<int32_t>(utf16.length()), &status);
    if (U_FAILURE(status)) {
        return std::string();
    }
    return result;
}

const char* EntityTypeToString(::TwitterText::EntityType type) noexcept {
    switch (type) {
        case ::TwitterText::EntityType::Url:
            return "url";
        case ::TwitterText::EntityType::ScreenName:
            return "mention";
        case ::TwitterText::EntityType::Hashtag:
            return "hashtag";
        case ::TwitterText::EntityType::ListName:
            return "listname";
        case ::TwitterText::EntityType::Symbol:
            return "cashtag";
        default:
            return "url";
    }
}

TwitterTextCodegen::Entity ToCodegenEntity(const std::u16string& text, const ::TwitterText::Entity& entity) {
    TwitterTextCodegen::Entity result;
    result.type = EntityTypeToString(entity.type);
    result.start = entity.start;
    result.end = entity.end;
    result.value = Utf8FromUtf16(text.substr(entity.start, entity.end - entity.start));

    std::optional<std::u16string> listSlug = ::TwitterText::ListSlugForEntity(text, entity);
    if (listSlug.has_value()) {
        result.listSlug = Utf8FromUtf16(*listSlug);
    }

    return result;
}

std::vector<TwitterTextCodegen::Entity> ToCodegenEntities(const std::u16string& text, const std::vector<::TwitterText::Entity>& entities) {
    std::vector<TwitterTextCodegen::Entity> results;
    results.reserve(entities.size());
    for (const auto& entity : entities) {
        results.push_back(ToCodegenEntity(text, entity));
    }
    return results;
}

} // namespace

TwitterTextCodegen::ParseResults TwitterTextModule::parseTweet(std::string text) noexcept {
    std::u16string utf16 = Utf16FromUtf8(text);
    ::TwitterText::ParseResults parsed = ::TwitterText::ParseTweet(utf16);

    TwitterTextCodegen::ParseResults result;
    result.weightedLength = static_cast<double>(parsed.weightedLength);
    result.permillage = static_cast<double>(parsed.permillage);
    result.isValid = parsed.isValid;
    result.displayTextRangeStart = static_cast<double>(parsed.displayTextRangeStart);
    result.displayTextRangeEnd = static_cast<double>(parsed.displayTextRangeEnd);
    result.validTextRangeStart = static_cast<double>(parsed.validTextRangeStart);
    result.validTextRangeEnd = static_cast<double>(parsed.validTextRangeEnd);
    return result;
}

std::vector<TwitterTextCodegen::Entity> TwitterTextModule::extractURLs(std::string text) noexcept {
    std::u16string utf16 = Utf16FromUtf8(text);
    return ToCodegenEntities(utf16, ::TwitterText::UrlsInText(utf16));
}

std::vector<TwitterTextCodegen::Entity> TwitterTextModule::extractHashtags(std::string text) noexcept {
    std::u16string utf16 = Utf16FromUtf8(text);
    return ToCodegenEntities(utf16, ::TwitterText::HashtagsInText(utf16, /*checkingUrlOverlap=*/true));
}

std::vector<TwitterTextCodegen::Entity> TwitterTextModule::extractMentions(std::string text) noexcept {
    std::u16string utf16 = Utf16FromUtf8(text);
    return ToCodegenEntities(utf16, ::TwitterText::MentionedScreenNamesInText(utf16));
}

std::vector<TwitterTextCodegen::Entity> TwitterTextModule::extractMentionsOrLists(std::string text) noexcept {
    std::u16string utf16 = Utf16FromUtf8(text);
    return ToCodegenEntities(utf16, ::TwitterText::MentionsOrListsInText(utf16));
}

std::vector<TwitterTextCodegen::Entity> TwitterTextModule::extractCashtags(std::string text) noexcept {
    std::u16string utf16 = Utf16FromUtf8(text);
    return ToCodegenEntities(utf16, ::TwitterText::SymbolsInText(utf16, /*checkingUrlOverlap=*/true));
}

std::vector<TwitterTextCodegen::Entity> TwitterTextModule::extractEntities(std::string text) noexcept {
    std::u16string utf16 = Utf16FromUtf8(text);
    return ToCodegenEntities(utf16, ::TwitterText::EntitiesInText(utf16));
}

std::optional<std::string> TwitterTextModule::extractReplyScreenname(std::string text) noexcept {
    std::u16string utf16 = Utf16FromUtf8(text);
    std::optional<::TwitterText::Entity> entity = ::TwitterText::RepliedScreenNameInText(utf16);
    if (!entity.has_value()) {
        return std::nullopt;
    }
    return Utf8FromUtf16(utf16.substr(entity->start, entity->end - entity->start));
}

bool TwitterTextModule::isValidTweet(std::string text) noexcept {
    std::u16string utf16 = Utf16FromUtf8(text);
    return ::TwitterText::ParseTweet(utf16).isValid;
}

bool TwitterTextModule::isValidHashtag(std::string text) noexcept {
    std::u16string utf16 = Utf16FromUtf8(text);
    return ::TwitterText::IsValidHashtagText(utf16);
}

double TwitterTextModule::tweetLength(std::string text) noexcept {
    std::u16string utf16 = Utf16FromUtf8(text);
    return static_cast<double>(::TwitterText::TweetLength(utf16));
}

} // namespace winrt::TwitterText
