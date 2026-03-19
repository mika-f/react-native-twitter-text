# react-native-twitter-text

A React Native plugin that provides Twitter's [twitter-text](https://github.com/twitter/twitter-text) parsing library via native Objective-C and Java implementations. Especially fast for CJK text compared to the JavaScript implementation.

## Features

- Tweet parsing with weighted character count (v3 configuration)
- Entity extraction (URLs, hashtags, @mentions, cashtags, lists)
- Tweet validation
- Synchronous API via React Native Turbo Modules (New Architecture)

## Installation

```sh
npm install react-native-twitter-text
cd ios && pod install
```

### Expo

```sh
npx expo install react-native-twitter-text
```

Add the plugin to your `app.json`:

```json
{
  "expo": {
    "plugins": ["react-native-twitter-text"]
  }
}
```

Then run prebuild:

```sh
npx expo prebuild
```

## Usage

### Parse a tweet

```ts
import { parseTweet } from 'react-native-twitter-text';

const result = parseTweet('Hello, world!');
// {
//   weightedLength: 13,
//   permillage: 46,
//   isValid: true,
//   displayTextRange: { start: 0, end: 13 },
//   validTextRange: { start: 0, end: 13 },
// }
```

### Extract entities

```ts
import { extractEntities } from 'react-native-twitter-text';

const entities = extractEntities('@jack Check out #ReactNative https://reactnative.dev');
// [
//   { type: 'mention', value: '@jack', range: { start: 0, end: 5 } },
//   { type: 'hashtag', value: '#ReactNative', range: { start: 16, end: 28 } },
//   { type: 'url', value: 'https://reactnative.dev', range: { start: 29, end: 52 } },
// ]
```

### Extract specific entity types

```ts
import {
  extractURLs,
  extractHashtags,
  extractMentions,
  extractMentionsOrLists,
  extractCashtags,
  extractReplyScreenname,
} from 'react-native-twitter-text';

extractURLs('Visit https://example.com');
extractHashtags('Hello #world');
extractMentions('Hi @user!');
extractMentionsOrLists('@user/my-list'); // includes listSlug
extractCashtags('Buy $AAPL');
extractReplyScreenname('@user hello'); // '@user' or null
```

### Validate tweets

```ts
import { isValidTweet, isValidHashtag, tweetLength } from 'react-native-twitter-text';

isValidTweet('Hello!');        // true
isValidTweet('');              // false
isValidHashtag('#ReactNative'); // true
tweetLength('Hello!');         // 6
```

## API Reference

### Types

```ts
type EntityType = 'url' | 'hashtag' | 'mention' | 'listname' | 'cashtag';

interface TextRange {
  start: number;
  end: number;
}

interface Entity {
  type: EntityType;
  value: string;
  range: TextRange;
  listSlug?: string; // present for 'listname' entities
}

interface ParseResults {
  weightedLength: number;
  permillage: number;
  isValid: boolean;
  displayTextRange: TextRange;
  validTextRange: TextRange;
}
```

### Functions

| Function | Return Type | Description |
|---|---|---|
| `parseTweet(text)` | `ParseResults` | Parse a tweet and return weighted length, validity, and text ranges |
| `extractEntities(text)` | `Entity[]` | Extract all entities (URLs, hashtags, mentions, cashtags) |
| `extractURLs(text)` | `Entity[]` | Extract URLs |
| `extractHashtags(text)` | `Entity[]` | Extract hashtags |
| `extractMentions(text)` | `Entity[]` | Extract @mentions |
| `extractMentionsOrLists(text)` | `Entity[]` | Extract @mentions and list references |
| `extractCashtags(text)` | `Entity[]` | Extract cashtags ($SYMBOL) |
| `extractReplyScreenname(text)` | `string \| null` | Extract the replied-to screen name |
| `isValidTweet(text)` | `boolean` | Check if a tweet is valid |
| `isValidHashtag(text)` | `boolean` | Check if text is a valid hashtag |
| `tweetLength(text)` | `number` | Get the weighted length of a tweet |

## Requirements

- React Native >= 0.76 (New Architecture / Turbo Modules)
- iOS >= 13.4
- Android minSdk >= 24

## Contributing

- [Development workflow](CONTRIBUTING.md#development-workflow)
- [Sending a pull request](CONTRIBUTING.md#sending-a-pull-request)
- [Code of conduct](CODE_OF_CONDUCT.md)

## License

MIT
