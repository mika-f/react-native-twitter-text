import TwitterText from './NativeTwitterText';
import type {
  Entity as NativeEntity,
  EntityType,
  ParseResults as NativeParseResults,
} from './NativeTwitterText';

export type { EntityType };

export interface TextRange {
  start: number;
  end: number;
}

export interface Entity {
  type: EntityType;
  value: string;
  range: TextRange;
  listSlug?: string;
}

export interface ParseResults {
  weightedLength: number;
  permillage: number;
  isValid: boolean;
  displayTextRange: TextRange;
  validTextRange: TextRange;
}

function toEntity(e: NativeEntity): Entity {
  return {
    type: e.type,
    value: e.value,
    range: { start: e.start, end: e.end },
    ...(e.listSlug != null ? { listSlug: e.listSlug } : {}),
  };
}

function toParseResults(r: NativeParseResults): ParseResults {
  return {
    weightedLength: r.weightedLength,
    permillage: r.permillage,
    isValid: r.isValid,
    displayTextRange: {
      start: r.displayTextRangeStart,
      end: r.displayTextRangeEnd,
    },
    validTextRange: {
      start: r.validTextRangeStart,
      end: r.validTextRangeEnd,
    },
  };
}

/**
 * Parse a tweet and return weighted length, validity, and text ranges.
 */
export function parseTweet(text: string): ParseResults {
  return toParseResults(TwitterText.parseTweet(text));
}

/**
 * Extract all entities (URLs, hashtags, mentions, cashtags) from text.
 */
export function extractEntities(text: string): Entity[] {
  return TwitterText.extractEntities(text).map(toEntity);
}

/**
 * Extract URLs from text.
 */
export function extractURLs(text: string): Entity[] {
  return TwitterText.extractURLs(text).map(toEntity);
}

/**
 * Extract hashtags from text.
 */
export function extractHashtags(text: string): Entity[] {
  return TwitterText.extractHashtags(text).map(toEntity);
}

/**
 * Extract @mentions from text.
 */
export function extractMentions(text: string): Entity[] {
  return TwitterText.extractMentions(text).map(toEntity);
}

/**
 * Extract @mentions and list references from text.
 */
export function extractMentionsOrLists(text: string): Entity[] {
  return TwitterText.extractMentionsOrLists(text).map(toEntity);
}

/**
 * Extract cashtags ($SYMBOL) from text.
 */
export function extractCashtags(text: string): Entity[] {
  return TwitterText.extractCashtags(text).map(toEntity);
}

/**
 * Extract the replied-to screen name from the beginning of text.
 * Returns null if the text is not a reply.
 */
export function extractReplyScreenname(text: string): string | null {
  return TwitterText.extractReplyScreenname(text);
}

/**
 * Check if a tweet is valid (within character limits, no invalid characters).
 */
export function isValidTweet(text: string): boolean {
  return TwitterText.isValidTweet(text);
}

/**
 * Check if text is a valid hashtag.
 */
export function isValidHashtag(text: string): boolean {
  return TwitterText.isValidHashtag(text);
}

/**
 * Get the weighted length of a tweet.
 */
export function tweetLength(text: string): number {
  return TwitterText.tweetLength(text);
}
