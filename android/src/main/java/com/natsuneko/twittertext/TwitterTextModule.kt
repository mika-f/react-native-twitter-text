package com.natsuneko.twittertext

import com.facebook.react.bridge.Arguments
import com.facebook.react.bridge.ReactApplicationContext
import com.facebook.react.bridge.ReadableArray
import com.facebook.react.bridge.ReadableMap
import com.facebook.react.bridge.WritableArray
import com.facebook.react.bridge.WritableMap
import com.twitter.twittertext.Extractor
import com.twitter.twittertext.TwitterTextParser
import com.twitter.twittertext.Validator

class TwitterTextModule(reactContext: ReactApplicationContext) :
  NativeTwitterTextSpec(reactContext) {

  private val extractor = Extractor()
  private val validator = Validator()

  companion object {
    const val NAME = NativeTwitterTextSpec.NAME

    private fun entityTypeToString(type: Extractor.Entity.Type): String {
      return when (type) {
        Extractor.Entity.Type.URL -> "url"
        Extractor.Entity.Type.HASHTAG -> "hashtag"
        Extractor.Entity.Type.MENTION -> "mention"
        Extractor.Entity.Type.CASHTAG -> "cashtag"
      }
    }

    private fun entityToMap(entity: Extractor.Entity): WritableMap {
      val map = Arguments.createMap()
      map.putString("type", entityTypeToString(entity.type))
      map.putInt("start", entity.start)
      map.putInt("end", entity.end)
      map.putString("value", entity.value)
      if (entity.listSlug != null) {
        map.putString("listSlug", entity.listSlug)
      }
      return map
    }

    private fun entitiesToArray(entities: List<Extractor.Entity>): WritableArray {
      val array = Arguments.createArray()
      for (entity in entities) {
        array.pushMap(entityToMap(entity))
      }
      return array
    }
  }

  // region Tweet Parsing

  override fun parseTweet(text: String): WritableMap {
    val results = TwitterTextParser.parseTweet(text)
    val map = Arguments.createMap()
    map.putInt("weightedLength", results.weightedLength)
    map.putInt("permillage", results.permillage)
    map.putBoolean("isValid", results.isValid)
    map.putInt("displayTextRangeStart", results.displayTextRange.start)
    map.putInt("displayTextRangeEnd", results.displayTextRange.end)
    map.putInt("validTextRangeStart", results.validTextRange.start)
    map.putInt("validTextRangeEnd", results.validTextRange.end)
    return map
  }

  // endregion

  // region Entity Extraction

  override fun extractEntities(text: String): WritableArray {
    val entities = extractor.extractEntitiesWithIndices(text)
    return entitiesToArray(entities)
  }

  override fun extractURLs(text: String): WritableArray {
    val entities = extractor.extractURLsWithIndices(text)
    return entitiesToArray(entities)
  }

  override fun extractHashtags(text: String): WritableArray {
    val entities = extractor.extractHashtagsWithIndices(text)
    return entitiesToArray(entities)
  }

  override fun extractMentions(text: String): WritableArray {
    val entities = extractor.extractMentionedScreennamesWithIndices(text)
    return entitiesToArray(entities)
  }

  override fun extractMentionsOrLists(text: String): WritableArray {
    val entities = extractor.extractMentionsOrListsWithIndices(text)
    return entitiesToArray(entities)
  }

  override fun extractCashtags(text: String): WritableArray {
    val entities = extractor.extractCashtagsWithIndices(text)
    return entitiesToArray(entities)
  }

  override fun extractReplyScreenname(text: String): String? {
    return extractor.extractReplyScreenname(text)
  }

  // endregion

  // region Validation

  override fun isValidTweet(text: String): Boolean {
    val results = TwitterTextParser.parseTweet(text)
    return results.isValid
  }

  override fun isValidHashtag(text: String): Boolean {
    return validator.isValidHashtag(text)
  }

  // endregion

  // region Tweet Length

  override fun tweetLength(text: String): Double {
    val results = TwitterTextParser.parseTweet(text)
    return results.weightedLength.toDouble()
  }

  // endregion
}
