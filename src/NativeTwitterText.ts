import { TurboModuleRegistry, type TurboModule } from 'react-native';

export type EntityType = 'url' | 'hashtag' | 'mention' | 'listname' | 'cashtag';

export interface Entity {
  type: EntityType;
  start: number;
  end: number;
  value: string;
  listSlug?: string;
}

export interface ParseResults {
  weightedLength: number;
  permillage: number;
  isValid: boolean;
  displayTextRangeStart: number;
  displayTextRangeEnd: number;
  validTextRangeStart: number;
  validTextRangeEnd: number;
}

export interface Spec extends TurboModule {
  // Tweet parsing
  parseTweet(text: string): ParseResults;

  // Entity extraction
  extractURLs(text: string): Entity[];
  extractHashtags(text: string): Entity[];
  extractMentions(text: string): Entity[];
  extractMentionsOrLists(text: string): Entity[];
  extractCashtags(text: string): Entity[];
  extractEntities(text: string): Entity[];
  extractReplyScreenname(text: string): string | null;

  // Validation
  isValidTweet(text: string): boolean;
  isValidHashtag(text: string): boolean;

  // Tweet length
  tweetLength(text: string): number;
}

export default TurboModuleRegistry.getEnforcing<Spec>('TwitterText');
