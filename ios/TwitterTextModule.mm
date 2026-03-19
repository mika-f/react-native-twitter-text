#import "TwitterTextModule.h"
#import "TwitterText.h"
#import "TwitterTextEntity.h"

static NSString *entityTypeToString(TwitterTextEntityType type) {
    switch (type) {
        case TwitterTextEntityURL:
            return @"url";
        case TwitterTextEntityScreenName:
            return @"mention";
        case TwitterTextEntityHashtag:
            return @"hashtag";
        case TwitterTextEntityListName:
            return @"listname";
        case TwitterTextEntitySymbol:
            return @"cashtag";
        default:
            return @"url";
    }
}

static NSDictionary *entityToDictionary(TwitterTextEntity *entity, NSString *text) {
    NSString *value = [text substringWithRange:entity.range];
    NSMutableDictionary *dict = [NSMutableDictionary dictionaryWithDictionary:@{
        @"type": entityTypeToString(entity.type),
        @"start": @(entity.range.location),
        @"end": @(entity.range.location + entity.range.length),
        @"value": value,
    }];

    // For list entities, extract the list slug
    if (entity.type == TwitterTextEntityListName) {
        NSRange slashRange = [value rangeOfString:@"/"];
        if (slashRange.location != NSNotFound) {
            dict[@"listSlug"] = [value substringFromIndex:slashRange.location];
        }
    }

    return [dict copy];
}

static NSArray<NSDictionary *> *entitiesToArray(NSArray<TwitterTextEntity *> *entities, NSString *text) {
    NSMutableArray *result = [NSMutableArray arrayWithCapacity:entities.count];
    for (TwitterTextEntity *entity in entities) {
        [result addObject:entityToDictionary(entity, text)];
    }
    return [result copy];
}

@implementation TwitterTextModule

+ (NSString *)moduleName
{
    return @"TwitterText";
}

RCT_EXTERN void RCTRegisterModule(Class);

+ (void)load
{
    RCTRegisterModule(self);

    // Eagerly initialize the default parser with v3 config.
    // We load from the main bundle to avoid resource lookup issues
    // when twitter-text's bundleForClass: resolves to a different bundle.
    NSString *configPath = [[NSBundle mainBundle] pathForResource:@"v3" ofType:@"json"];
    if (configPath) {
        NSString *jsonString = [NSString stringWithContentsOfFile:configPath encoding:NSUTF8StringEncoding error:nil];
        if (jsonString) {
            TwitterTextConfiguration *config = [TwitterTextConfiguration configurationFromJSONString:jsonString];
            [TwitterTextParser setDefaultParserWithConfiguration:config];
        }
    }
}

#pragma mark - Tweet Parsing

- (NSDictionary *)parseTweet:(NSString *)text {
    TwitterTextParser *parser = [TwitterTextParser defaultParser];
    TwitterTextParseResults *results = [parser parseTweet:text];

    return @{
        @"weightedLength": @(results.weightedLength),
        @"permillage": @(results.permillage),
        @"isValid": @(results.isValid),
        @"displayTextRangeStart": @(results.displayTextRange.location),
        @"displayTextRangeEnd": @(results.displayTextRange.location + results.displayTextRange.length),
        @"validTextRangeStart": @(results.validDisplayTextRange.location),
        @"validTextRangeEnd": @(results.validDisplayTextRange.location + results.validDisplayTextRange.length),
    };
}

#pragma mark - Entity Extraction

- (NSArray<NSDictionary *> *)extractEntities:(NSString *)text {
    NSArray<TwitterTextEntity *> *entities = [TwitterText entitiesInText:text];
    return entitiesToArray(entities, text);
}

- (NSArray<NSDictionary *> *)extractURLs:(NSString *)text {
    NSArray<TwitterTextEntity *> *entities = [TwitterText URLsInText:text];
    return entitiesToArray(entities, text);
}

- (NSArray<NSDictionary *> *)extractHashtags:(NSString *)text {
    NSArray<TwitterTextEntity *> *entities = [TwitterText hashtagsInText:text checkingURLOverlap:YES];
    return entitiesToArray(entities, text);
}

- (NSArray<NSDictionary *> *)extractMentions:(NSString *)text {
    NSArray<TwitterTextEntity *> *entities = [TwitterText mentionedScreenNamesInText:text];
    return entitiesToArray(entities, text);
}

- (NSArray<NSDictionary *> *)extractMentionsOrLists:(NSString *)text {
    NSArray<TwitterTextEntity *> *entities = [TwitterText mentionsOrListsInText:text];
    return entitiesToArray(entities, text);
}

- (NSArray<NSDictionary *> *)extractCashtags:(NSString *)text {
    NSArray<TwitterTextEntity *> *entities = [TwitterText symbolsInText:text checkingURLOverlap:YES];
    return entitiesToArray(entities, text);
}

- (nullable NSString *)extractReplyScreenname:(NSString *)text {
    TwitterTextEntity *entity = [TwitterText repliedScreenNameInText:text];
    if (entity == nil) {
        return nil;
    }
    return [text substringWithRange:entity.range];
}

#pragma mark - Validation

- (NSNumber *)isValidTweet:(NSString *)text {
    TwitterTextParser *parser = [TwitterTextParser defaultParser];
    TwitterTextParseResults *results = [parser parseTweet:text];
    return @(results.isValid);
}

- (NSNumber *)isValidHashtag:(NSString *)text {
    NSString *candidate = text;
    if (![candidate hasPrefix:@"#"]) {
        candidate = [@"#" stringByAppendingString:candidate];
    }
    NSArray<TwitterTextEntity *> *hashtags = [TwitterText hashtagsInText:candidate checkingURLOverlap:YES];
    if (hashtags.count != 1) {
        return @NO;
    }
    TwitterTextEntity *entity = hashtags.firstObject;
    return @(entity.range.location == 0 && entity.range.length == candidate.length);
}

#pragma mark - Tweet Length

- (NSNumber *)tweetLength:(NSString *)text {
    return @([TwitterText tweetLength:text]);
}

#pragma mark - Turbo Module

- (std::shared_ptr<facebook::react::TurboModule>)getTurboModule:
    (const facebook::react::ObjCTurboModule::InitParams &)params
{
    return std::make_shared<facebook::react::NativeTwitterTextSpecJSI>(params);
}

@end
