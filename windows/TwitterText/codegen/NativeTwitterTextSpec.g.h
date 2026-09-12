/*
 * This file is auto-generated from a NativeModule spec file in js.
 *
 * This is a C++ Spec class that should be used with MakeTurboModuleProvider to register native modules
 * in a way that also verifies at compile time that the native module matches the interface required
 * by the TurboModule JS spec.
 */
#pragma once
// clang-format off

inline winrt::Microsoft::ReactNative::FieldMap GetStructInfo(TwitterTextCodegen::ParseResults*) noexcept {
    winrt::Microsoft::ReactNative::FieldMap fieldMap {
        {L"weightedLength", &TwitterTextCodegen::ParseResults::weightedLength},
        {L"permillage", &TwitterTextCodegen::ParseResults::permillage},
        {L"isValid", &TwitterTextCodegen::ParseResults::isValid},
        {L"displayTextRangeStart", &TwitterTextCodegen::ParseResults::displayTextRangeStart},
        {L"displayTextRangeEnd", &TwitterTextCodegen::ParseResults::displayTextRangeEnd},
        {L"validTextRangeStart", &TwitterTextCodegen::ParseResults::validTextRangeStart},
        {L"validTextRangeEnd", &TwitterTextCodegen::ParseResults::validTextRangeEnd},
    };
    return fieldMap;
}

inline winrt::Microsoft::ReactNative::FieldMap GetStructInfo(TwitterTextCodegen::Entity*) noexcept {
    winrt::Microsoft::ReactNative::FieldMap fieldMap {
        {L"type", &TwitterTextCodegen::Entity::type},
        {L"start", &TwitterTextCodegen::Entity::start},
        {L"end", &TwitterTextCodegen::Entity::end},
        {L"value", &TwitterTextCodegen::Entity::value},
        {L"listSlug", &TwitterTextCodegen::Entity::listSlug},
    };
    return fieldMap;
}

// #include "NativeTwitterTextDataTypes.g.h" before this file to use the generated type definition
#include <NativeModules.h>
#include <tuple>

namespace TwitterTextCodegen {

struct TwitterTextSpec : winrt::Microsoft::ReactNative::TurboModuleSpec {
  static constexpr auto methods = std::tuple{
      SyncMethod<ParseResults(std::string) noexcept>{0, L"parseTweet"},
      SyncMethod<std::vector<Entity>(std::string) noexcept>{1, L"extractURLs"},
      SyncMethod<std::vector<Entity>(std::string) noexcept>{2, L"extractHashtags"},
      SyncMethod<std::vector<Entity>(std::string) noexcept>{3, L"extractMentions"},
      SyncMethod<std::vector<Entity>(std::string) noexcept>{4, L"extractMentionsOrLists"},
      SyncMethod<std::vector<Entity>(std::string) noexcept>{5, L"extractCashtags"},
      SyncMethod<std::vector<Entity>(std::string) noexcept>{6, L"extractEntities"},
      SyncMethod<std::optional<std::string>(std::string) noexcept>{7, L"extractReplyScreenname"},
      SyncMethod<bool(std::string) noexcept>{8, L"isValidTweet"},
      SyncMethod<bool(std::string) noexcept>{9, L"isValidHashtag"},
      SyncMethod<double(std::string) noexcept>{10, L"tweetLength"},
  };

  template <class TModule>
  static constexpr void ValidateModule() noexcept {
    constexpr auto methodCheckResults = CheckMethods<TModule, TwitterTextSpec>();

    REACT_SHOW_METHOD_SPEC_ERRORS(
          0,
          "parseTweet",
          "    REACT_SYNC_METHOD(parseTweet) ParseResults parseTweet(std::string text) noexcept { /* implementation */ }\n"
          "    REACT_SYNC_METHOD(parseTweet) static ParseResults parseTweet(std::string text) noexcept { /* implementation */ }\n");
    REACT_SHOW_METHOD_SPEC_ERRORS(
          1,
          "extractURLs",
          "    REACT_SYNC_METHOD(extractURLs) std::vector<Entity> extractURLs(std::string text) noexcept { /* implementation */ }\n"
          "    REACT_SYNC_METHOD(extractURLs) static std::vector<Entity> extractURLs(std::string text) noexcept { /* implementation */ }\n");
    REACT_SHOW_METHOD_SPEC_ERRORS(
          2,
          "extractHashtags",
          "    REACT_SYNC_METHOD(extractHashtags) std::vector<Entity> extractHashtags(std::string text) noexcept { /* implementation */ }\n"
          "    REACT_SYNC_METHOD(extractHashtags) static std::vector<Entity> extractHashtags(std::string text) noexcept { /* implementation */ }\n");
    REACT_SHOW_METHOD_SPEC_ERRORS(
          3,
          "extractMentions",
          "    REACT_SYNC_METHOD(extractMentions) std::vector<Entity> extractMentions(std::string text) noexcept { /* implementation */ }\n"
          "    REACT_SYNC_METHOD(extractMentions) static std::vector<Entity> extractMentions(std::string text) noexcept { /* implementation */ }\n");
    REACT_SHOW_METHOD_SPEC_ERRORS(
          4,
          "extractMentionsOrLists",
          "    REACT_SYNC_METHOD(extractMentionsOrLists) std::vector<Entity> extractMentionsOrLists(std::string text) noexcept { /* implementation */ }\n"
          "    REACT_SYNC_METHOD(extractMentionsOrLists) static std::vector<Entity> extractMentionsOrLists(std::string text) noexcept { /* implementation */ }\n");
    REACT_SHOW_METHOD_SPEC_ERRORS(
          5,
          "extractCashtags",
          "    REACT_SYNC_METHOD(extractCashtags) std::vector<Entity> extractCashtags(std::string text) noexcept { /* implementation */ }\n"
          "    REACT_SYNC_METHOD(extractCashtags) static std::vector<Entity> extractCashtags(std::string text) noexcept { /* implementation */ }\n");
    REACT_SHOW_METHOD_SPEC_ERRORS(
          6,
          "extractEntities",
          "    REACT_SYNC_METHOD(extractEntities) std::vector<Entity> extractEntities(std::string text) noexcept { /* implementation */ }\n"
          "    REACT_SYNC_METHOD(extractEntities) static std::vector<Entity> extractEntities(std::string text) noexcept { /* implementation */ }\n");
    REACT_SHOW_METHOD_SPEC_ERRORS(
          7,
          "extractReplyScreenname",
          "    REACT_SYNC_METHOD(extractReplyScreenname) std::optional<std::string> extractReplyScreenname(std::string text) noexcept { /* implementation */ }\n"
          "    REACT_SYNC_METHOD(extractReplyScreenname) static std::optional<std::string> extractReplyScreenname(std::string text) noexcept { /* implementation */ }\n");
    REACT_SHOW_METHOD_SPEC_ERRORS(
          8,
          "isValidTweet",
          "    REACT_SYNC_METHOD(isValidTweet) bool isValidTweet(std::string text) noexcept { /* implementation */ }\n"
          "    REACT_SYNC_METHOD(isValidTweet) static bool isValidTweet(std::string text) noexcept { /* implementation */ }\n");
    REACT_SHOW_METHOD_SPEC_ERRORS(
          9,
          "isValidHashtag",
          "    REACT_SYNC_METHOD(isValidHashtag) bool isValidHashtag(std::string text) noexcept { /* implementation */ }\n"
          "    REACT_SYNC_METHOD(isValidHashtag) static bool isValidHashtag(std::string text) noexcept { /* implementation */ }\n");
    REACT_SHOW_METHOD_SPEC_ERRORS(
          10,
          "tweetLength",
          "    REACT_SYNC_METHOD(tweetLength) double tweetLength(std::string text) noexcept { /* implementation */ }\n"
          "    REACT_SYNC_METHOD(tweetLength) static double tweetLength(std::string text) noexcept { /* implementation */ }\n");
  }
};

} // namespace TwitterTextCodegen
