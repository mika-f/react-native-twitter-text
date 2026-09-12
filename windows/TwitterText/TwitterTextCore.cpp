// C++/ICU port of the twitter-text parsing algorithm for react-native-windows.
//
// IMPORTANT: Windows only exposes ICU's plain C API (<icu.h> / icu.lib) -- it
// does not expose (and, per Microsoft's own documentation, never will expose)
// the ICU C++ classes (icu::UnicodeString, icu::RegexMatcher, icu::IDNA, ...)
// because there is no stable C++ ABI. See:
// https://learn.microsoft.com/en-us/windows/win32/intl/international-components-for-unicode--icu-
// Every ICU call below therefore goes through the C API, operating on
// std::u16string buffers (UChar === char16_t on Windows's <icu.h>).
//
// The regular expressions below are transcribed verbatim (character-for-character)
// from submodules/twitter-text/objc/lib/TwitterText.m and TwitterTextEmoji.h, with
// every embedded Unicode literal normalized to an explicit "\\uXXXX"/"\\UXXXXXXXX"
// ICU regex escape (rather than relying on compile-time universal-character-name
// embedding, which is not reliably portable across MSVC configurations). The
// weighted-length / parseTweet algorithm follows the structure of
// submodules/twitter-text/java/src/main/java/com/twitter/twittertext/TwitterTextParser.java,
// which is simpler to port faithfully than the Objective-C composed-character-sequence
// based version while producing equivalent results.
//
// DO NOT hand-edit the regex bodies below without cross-checking against the
// Objective-C/Java sources -- they are extremely sensitive to exact escaping.

#include "pch.h"
#include "TwitterTextCore.h"
#include "TwitterTextConfig.h"

#include <icu.h>

#include <algorithm>
#include <memory>
#include <utility>

// clang-format off
#define TWUControlCharacters        "\\u0009-\\u000D"
#define TWUSpace                    "\\u0020"
#define TWUControl85                "\\u0085"
#define TWUNoBreakSpace             "\\u00A0"
#define TWUOghamBreakSpace          "\\u1680"
#define TWUMongolianVowelSeparator  "\\u180E"
#define TWUWhiteSpaces              "\\u2000-\\u200A"
#define TWULineSeparator            "\\u2028"
#define TWUParagraphSeparator       "\\u2029"
#define TWUNarrowNoBreakSpace       "\\u202F"
#define TWUMediumMathematicalSpace  "\\u205F"
#define TWUIdeographicSpace         "\\u3000"

#define TWUUnicodeSpaces \
    TWUControlCharacters \
    TWUSpace \
    TWUControl85 \
    TWUNoBreakSpace \
    TWUOghamBreakSpace \
    TWUMongolianVowelSeparator \
    TWUWhiteSpaces \
    TWULineSeparator \
    TWUParagraphSeparator \
    TWUNarrowNoBreakSpace \
    TWUMediumMathematicalSpace \
    TWUIdeographicSpace

#define TWUUnicodeALM               "\\u061C"
#define TWUUnicodeLRM               "\\u200E"
#define TWUUnicodeRLM               "\\u200F"
#define TWUUnicodeLRE               "\\u202A"
#define TWUUnicodeRLE               "\\u202B"
#define TWUUnicodePDF               "\\u202C"
#define TWUUnicodeLRO               "\\u202D"
#define TWUUnicodeRLO               "\\u202E"
#define TWUUnicodeLRI               "\\u2066"
#define TWUUnicodeRLI               "\\u2067"
#define TWUUnicodeFSI               "\\u2068"
#define TWUUnicodePDI               "\\u2069"

#define TWUUnicodeDirectionalCharacters \
    TWUUnicodeALM \
    TWUUnicodeLRM \
    TWUUnicodeRLM \
    TWUUnicodeLRE \
    TWUUnicodeRLE \
    TWUUnicodePDF \
    TWUUnicodeLRO \
    TWUUnicodeRLO \
    TWUUnicodeLRI \
    TWUUnicodeRLI \
    TWUUnicodeFSI \
    TWUUnicodePDI

#define TWUInvalidCharacters        "\\uFFFE\\uFEFF\\uFFFF"
#define TWUInvalidCharactersPattern "[" TWUInvalidCharacters "]"

#define TWULatinAccents \
    "\\u00C0-\\u00D6\\u00D8-\\u00F6\\u00F8-\\u00FF\\u0100-\\u024F\\u0253-\\u0254\\u0256-\\u0257\\u0259\\u025b\\u0263\\u0268\\u026F\\u0272\\u0289\\u02BB\\u1E00-\\u1EFF"

//
// Hashtag
//

#define TWUPunctuationChars                             "-_!\"#$%&'\\(\\)*+,./:;<=>?@\\[\\]^`\\{|}~"
#define TWUPunctuationCharsWithoutHyphen                "_!\"#$%&'\\(\\)*+,./:;<=>?@\\[\\]^`\\{|}~"
#define TWUPunctuationCharsWithoutHyphenAndUnderscore   "!\"#$%&'\\(\\)*+,./:;<=>?@\\[\\]^`\\{|}~"

#define TWHashtagAlpha                          "[\\p{L}\\p{M}]"
#define TWHashtagSpecialChars                   "_\\u200c\\u200d\\ua67e\\u05be\\u05f3\\u05f4\\uff5e\\u301c\\u309b\\u309c\\u30a0\\u30fb\\u3003\\u0f0b\\u0f0c\\u00b7"
#define TWUHashtagAlphanumeric                  "[\\p{L}\\p{M}\\p{Nd}" TWHashtagSpecialChars "]"
#define TWUHashtagBoundaryInvalidChars          "&\\p{L}\\p{M}\\p{Nd}" TWHashtagSpecialChars

#define TWUHashtagBoundary \
"^|\\ufe0e|\\ufe0f|$|[^" \
    TWUHashtagBoundaryInvalidChars \
"]"

#define TWUValidHashtag \
    "(?:" TWUHashtagBoundary ")([#＃](?!\\ufe0f|\\u20e3)" TWUHashtagAlphanumeric "*" TWHashtagAlpha TWUHashtagAlphanumeric "*)"

#define TWUEndHashTagMatch      "\\A(?:[#＃]|://)"

//
// Symbol (Cashtag)
//

#define TWUSymbol               "[a-z]{1,6}(?:[._][a-z]{1,2})?"
#define TWUValidSymbol \
    "(?:^|[" TWUUnicodeSpaces TWUUnicodeDirectionalCharacters "])" \
    "(\\$" TWUSymbol ")" \
    "(?=$|\\s|[" TWUPunctuationChars "])"

//
// Mention and list name
//

#define TWUValidMentionPrecedingChars   "(?:[^a-z0-9_!#$%&*@＠]|^|(?:^|[^a-z0-9_+~.-])RT:?)"
#define TWUAtSigns                      "[@＠]"
#define TWUValidUsername                "\\A" TWUAtSigns "[a-z0-9_]{1,20}\\z"
#define TWUValidList                    "\\A" TWUAtSigns "[a-z0-9_]{1,20}/[a-z][a-z0-9_\\-]{0,24}\\z"

#define TWUValidMentionOrList \
    "(" TWUValidMentionPrecedingChars ")" \
    "(" TWUAtSigns ")" \
    "([a-z0-9_]{1,20})" \
    "(/[a-z][a-z0-9_\\-]{0,24})?"

#define TWUValidReply                   "\\A(?:[" TWUUnicodeSpaces TWUUnicodeDirectionalCharacters "])*" TWUAtSigns "([a-z0-9_]{1,20})"
#define TWUEndMentionMatch              "\\A(?:" TWUAtSigns "|[" TWULatinAccents "]|://)"

//
// URL
//

#define TWUValidURLPrecedingChars       "(?:[^a-z0-9@＠$#＃" TWUInvalidCharacters "]|[" TWUUnicodeDirectionalCharacters "]|^)"

// These patterns extract domains that are ascii+latin only. We separately check
// for unencoded domains with unicode characters elsewhere.
#define TWUValidURLCharacters           "[a-z0-9" TWULatinAccents "]"
#define TWUValidURLSubdomain            "(?>(?:" TWUValidURLCharacters "[" TWUValidURLCharacters "\\-_]{0,255})?" TWUValidURLCharacters "\\.)"
#define TWUValidURLDomain               "(?:(?:" TWUValidURLCharacters "[" TWUValidURLCharacters "\\-]{0,255})?" TWUValidURLCharacters "\\.)"

// Used to extract domains that contain unencoded unicode.
#define TWUValidURLUnicodeCharacters \
"[^" \
    TWUPunctuationChars \
    "\\s\\p{Z}\\p{InGeneralPunctuation}" \
"]"

#define TWUValidURLUnicodeDomain        "(?:(?:" TWUValidURLUnicodeCharacters "[" TWUValidURLUnicodeCharacters "\\-]{0,255})?" TWUValidURLUnicodeCharacters "\\.)"

#define TWUValidPunycode                "(?:xn--[-0-9a-z]+)"

#define TWUValidDomain \
"(?:" \
    TWUValidURLSubdomain "*" TWUValidURLDomain \
    "(?:" TWUValidGTLD "|" TWUValidCCTLD "|" TWUValidPunycode ")" \
")" \
"|(?:(?<=https?://)" \
  "(?:" \
    "(?:" TWUValidURLDomain TWUValidCCTLD ")" \
    "|(?:" \
      TWUValidURLUnicodeDomain "{0,255}" TWUValidURLUnicodeDomain \
      "(?:" TWUValidGTLD "|" TWUValidCCTLD ")" \
    ")" \
  ")" \
")" \
"|(?:" \
  TWUValidURLDomain TWUValidCCTLD "(?=/)" \
")"

#define TWUValidPortNumber              "[0-9]++"
#define TWUValidGeneralURLPathChars     "[a-z\\p{Cyrillic}0-9!\\*';:=+,.$/%#\\[\\]\\-\\u2013_~&|@" TWULatinAccents "]"

#define TWUValidURLBalancedParens               \
"\\(" \
    "(?:" \
        TWUValidGeneralURLPathChars "+" \
        "|" \
        "(?:" \
          TWUValidGeneralURLPathChars "*" \
          "\\(" \
            TWUValidGeneralURLPathChars "+" \
          "\\)" \
        TWUValidGeneralURLPathChars "*" \
      ")" \
    ")" \
"\\)"

#define TWUValidURLPathEndingChars      "[a-z\\p{Cyrillic}0-9=_#/+\\-" TWULatinAccents "]|(?:" TWUValidURLBalancedParens ")"

#define TWUValidPath "(?:" \
  "(?:" \
    TWUValidGeneralURLPathChars "*" \
    "(?:" TWUValidURLBalancedParens TWUValidGeneralURLPathChars "*)*" \
    TWUValidURLPathEndingChars \
  ")|(?:@" TWUValidGeneralURLPathChars "+/)" \
")"

#define TWUValidURLQueryChars           "[a-z0-9!?*'\\(\\);:&=+$/%#\\[\\]\\-_\\.,~|@]"
#define TWUValidURLQueryEndingChars     "[a-z0-9\\-_&=#/]"

#define TWUValidURLPatternString \
"(" \
  "(" TWUValidURLPrecedingChars ")" \
  "(" \
    "(https?://)?" \
    "(" TWUValidDomain ")" \
    "(?::(" TWUValidPortNumber "))?" \
    "(/" \
      TWUValidPath "*+" \
    ")?" \
    "(\\?" TWUValidURLQueryChars "*" \
            TWUValidURLQueryEndingChars ")?" \
  ")" \
")"

enum TWUValidURLGroup {
    TWUValidURLGroupAll = 1,
    TWUValidURLGroupPreceding,
    TWUValidURLGroupURL,
    TWUValidURLGroupProtocol,
    TWUValidURLGroupDomain,
    TWUValidURLGroupPort,
    TWUValidURLGroupPath,
    TWUValidURLGroupQueryString
};

#define TWUValidGTLD \
"(?:(?:" \
    "삼성|닷컴|닷넷|香格里拉|餐厅|食品|飞利浦|電訊盈科|集团|通販|购物|谷歌|诺基亚|联通|网络|网站|网店|网址|组织机构|移动|珠宝|点看|游戏|淡马锡|机构|書籍|时尚|新闻|政府|政务|" \
    "招聘|手表|手机|我爱你|慈善|微博|广东|工行|家電|娱乐|天主教|大拿|大众汽车|在线|嘉里大酒店|嘉里|商标|商店|商城|公益|公司|八卦|健康|信息|佛山|企业|中文网|中信|世界|ポイント|" \
    "ファッション|セール|ストア|コム|グーグル|クラウド|みんな|คอม|संगठन|नेट|कॉम|همراه|موقع|موبايلي|كوم|كاثوليك|عرب|شبكة|بيتك|بازار|" \
    "العليان|ارامكو|اتصالات|ابوظبي|קום|сайт|рус|орг|онлайн|москва|ком|католик|дети|zuerich|zone|zippo|zip|" \
    "zero|zara|zappos|yun|youtube|you|yokohama|yoga|yodobashi|yandex|yamaxun|yahoo|yachts|xyz|xxx|xperia|" \
    "xin|xihuan|xfinity|xerox|xbox|wtf|wtc|wow|world|works|work|woodside|wolterskluwer|wme|winners|wine|" \
    "windows|win|williamhill|wiki|wien|whoswho|weir|weibo|wedding|wed|website|weber|webcam|weatherchannel|" \
    "weather|watches|watch|warman|wanggou|wang|walter|walmart|wales|vuelos|voyage|voto|voting|vote|volvo|" \
    "volkswagen|vodka|vlaanderen|vivo|viva|vistaprint|vista|vision|visa|virgin|vip|vin|villas|viking|vig|" \
    "video|viajes|vet|versicherung|vermögensberatung|vermögensberater|verisign|ventures|vegas|vanguard|" \
    "vana|vacations|ups|uol|uno|university|unicom|uconnect|ubs|ubank|tvs|tushu|tunes|tui|tube|trv|trust|" \
    "travelersinsurance|travelers|travelchannel|travel|training|trading|trade|toys|toyota|town|tours|" \
    "total|toshiba|toray|top|tools|tokyo|today|tmall|tkmaxx|tjx|tjmaxx|tirol|tires|tips|tiffany|tienda|" \
    "tickets|tiaa|theatre|theater|thd|teva|tennis|temasek|telefonica|telecity|tel|technology|tech|team|" \
    "tdk|tci|taxi|tax|tattoo|tatar|tatamotors|target|taobao|talk|taipei|tab|systems|symantec|sydney|swiss|" \
    "swiftcover|swatch|suzuki|surgery|surf|support|supply|supplies|sucks|style|study|studio|stream|store|" \
    "storage|stockholm|stcgroup|stc|statoil|statefarm|statebank|starhub|star|staples|stada|srt|srl|" \
    "spreadbetting|spot|sport|spiegel|space|soy|sony|song|solutions|solar|sohu|software|softbank|social|" \
    "soccer|sncf|smile|smart|sling|skype|sky|skin|ski|site|singles|sina|silk|shriram|showtime|show|shouji|" \
    "shopping|shop|shoes|shiksha|shia|shell|shaw|sharp|shangrila|sfr|sexy|sex|sew|seven|ses|services|" \
    "sener|select|seek|security|secure|seat|search|scot|scor|scjohnson|science|schwarz|schule|school|" \
    "scholarships|schmidt|schaeffler|scb|sca|sbs|sbi|saxo|save|sas|sarl|sapo|sap|sanofi|sandvikcoromant|" \
    "sandvik|samsung|samsclub|salon|sale|sakura|safety|safe|saarland|ryukyu|rwe|run|ruhr|rugby|rsvp|room|" \
    "rogers|rodeo|rocks|rocher|rmit|rip|rio|ril|rightathome|ricoh|richardli|rich|rexroth|reviews|review|" \
    "restaurant|rest|republican|report|repair|rentals|rent|ren|reliance|reit|reisen|reise|rehab|" \
    "redumbrella|redstone|red|recipes|realty|realtor|realestate|read|raid|radio|racing|qvc|quest|quebec|" \
    "qpon|pwc|pub|prudential|pru|protection|property|properties|promo|progressive|prof|productions|prod|" \
    "pro|prime|press|praxi|pramerica|post|porn|politie|poker|pohl|pnc|plus|plumbing|playstation|play|" \
    "place|pizza|pioneer|pink|ping|pin|pid|pictures|pictet|pics|piaget|physio|photos|photography|photo|" \
    "phone|philips|phd|pharmacy|pfizer|pet|pccw|pay|passagens|party|parts|partners|pars|paris|panerai|" \
    "panasonic|pamperedchef|page|ovh|ott|otsuka|osaka|origins|orientexpress|organic|org|orange|oracle|" \
    "open|ooo|onyourside|online|onl|ong|one|omega|ollo|oldnavy|olayangroup|olayan|okinawa|office|off|" \
    "observer|obi|nyc|ntt|nrw|nra|nowtv|nowruz|now|norton|northwesternmutual|nokia|nissay|nissan|ninja|" \
    "nikon|nike|nico|nhk|ngo|nfl|nexus|nextdirect|next|news|newholland|new|neustar|network|netflix|" \
    "netbank|net|nec|nba|navy|natura|nationwide|name|nagoya|nadex|nab|mutuelle|mutual|museum|mtr|mtpc|mtn|" \
    "msd|movistar|movie|mov|motorcycles|moto|moscow|mortgage|mormon|mopar|montblanc|monster|money|monash|" \
    "mom|moi|moe|moda|mobily|mobile|mobi|mma|mls|mlb|mitsubishi|mit|mint|mini|mil|microsoft|miami|metlife|" \
    "merckmsd|meo|menu|men|memorial|meme|melbourne|meet|media|med|mckinsey|mcdonalds|mcd|mba|mattel|" \
    "maserati|marshalls|marriott|markets|marketing|market|map|mango|management|man|makeup|maison|maif|" \
    "madrid|macys|luxury|luxe|lupin|lundbeck|ltda|ltd|lplfinancial|lpl|love|lotto|lotte|london|lol|loft|" \
    "locus|locker|loans|loan|llp|llc|lixil|living|live|lipsy|link|linde|lincoln|limo|limited|lilly|like|" \
    "lighting|lifestyle|lifeinsurance|life|lidl|liaison|lgbt|lexus|lego|legal|lefrak|leclerc|lease|lds|" \
    "lawyer|law|latrobe|latino|lat|lasalle|lanxess|landrover|land|lancome|lancia|lancaster|lamer|" \
    "lamborghini|ladbrokes|lacaixa|kyoto|kuokgroup|kred|krd|kpn|kpmg|kosher|komatsu|koeln|kiwi|kitchen|" \
    "kindle|kinder|kim|kia|kfh|kerryproperties|kerrylogistics|kerryhotels|kddi|kaufen|juniper|juegos|jprs|" \
    "jpmorgan|joy|jot|joburg|jobs|jnj|jmp|jll|jlc|jio|jewelry|jetzt|jeep|jcp|jcb|java|jaguar|iwc|iveco|" \
    "itv|itau|istanbul|ist|ismaili|iselect|irish|ipiranga|investments|intuit|international|intel|int|" \
    "insure|insurance|institute|ink|ing|info|infiniti|industries|inc|immobilien|immo|imdb|imamat|ikano|" \
    "iinet|ifm|ieee|icu|ice|icbc|ibm|hyundai|hyatt|hughes|htc|hsbc|how|house|hotmail|hotels|hoteles|hot|" \
    "hosting|host|hospital|horse|honeywell|honda|homesense|homes|homegoods|homedepot|holiday|holdings|" \
    "hockey|hkt|hiv|hitachi|hisamitsu|hiphop|hgtv|hermes|here|helsinki|help|healthcare|health|hdfcbank|" \
    "hdfc|hbo|haus|hangout|hamburg|hair|guru|guitars|guide|guge|gucci|guardian|group|grocery|gripe|green|" \
    "gratis|graphics|grainger|gov|got|gop|google|goog|goodyear|goodhands|goo|golf|goldpoint|gold|godaddy|" \
    "gmx|gmo|gmbh|gmail|globo|global|gle|glass|glade|giving|gives|gifts|gift|ggee|george|genting|gent|gea|" \
    "gdn|gbiz|gay|garden|gap|games|game|gallup|gallo|gallery|gal|fyi|futbol|furniture|fund|fun|fujixerox|" \
    "fujitsu|ftr|frontier|frontdoor|frogans|frl|fresenius|free|fox|foundation|forum|forsale|forex|ford|" \
    "football|foodnetwork|food|foo|fly|flsmidth|flowers|florist|flir|flights|flickr|fitness|fit|fishing|" \
    "fish|firmdale|firestone|fire|financial|finance|final|film|fido|fidelity|fiat|ferrero|ferrari|" \
    "feedback|fedex|fast|fashion|farmers|farm|fans|fan|family|faith|fairwinds|fail|fage|extraspace|" \
    "express|exposed|expert|exchange|everbank|events|eus|eurovision|etisalat|esurance|estate|esq|erni|" \
    "ericsson|equipment|epson|epost|enterprises|engineering|engineer|energy|emerck|email|education|edu|" \
    "edeka|eco|eat|earth|dvr|dvag|durban|dupont|duns|dunlop|duck|dubai|dtv|drive|download|dot|doosan|" \
    "domains|doha|dog|dodge|doctor|docs|dnp|diy|dish|discover|discount|directory|direct|digital|diet|" \
    "diamonds|dhl|dev|design|desi|dentist|dental|democrat|delta|deloitte|dell|delivery|degree|deals|" \
    "dealer|deal|dds|dclk|day|datsun|dating|date|data|dance|dad|dabur|cyou|cymru|cuisinella|csc|cruises|" \
    "cruise|crs|crown|cricket|creditunion|creditcard|credit|cpa|courses|coupons|coupon|country|corsica|" \
    "coop|cool|cookingchannel|cooking|contractors|contact|consulting|construction|condos|comsec|computer|" \
    "compare|company|community|commbank|comcast|com|cologne|college|coffee|codes|coach|clubmed|club|cloud|" \
    "clothing|clinique|clinic|click|cleaning|claims|cityeats|city|citic|citi|citadel|cisco|circle|" \
    "cipriani|church|chrysler|chrome|christmas|chloe|chintai|cheap|chat|chase|charity|channel|chanel|cfd|" \
    "cfa|cern|ceo|center|ceb|cbs|cbre|cbn|cba|catholic|catering|cat|casino|cash|caseih|case|casa|cartier|" \
    "cars|careers|career|care|cards|caravan|car|capitalone|capital|capetown|canon|cancerresearch|camp|" \
    "camera|cam|calvinklein|call|cal|cafe|cab|bzh|buzz|buy|business|builders|build|bugatti|budapest|" \
    "brussels|brother|broker|broadway|bridgestone|bradesco|box|boutique|bot|boston|bostik|bosch|boots|" \
    "booking|book|boo|bond|bom|bofa|boehringer|boats|bnpparibas|bnl|bmw|bms|blue|bloomberg|blog|" \
    "blockbuster|blanco|blackfriday|black|biz|bio|bingo|bing|bike|bid|bible|bharti|bet|bestbuy|best|" \
    "berlin|bentley|beer|beauty|beats|bcn|bcg|bbva|bbt|bbc|bayern|bauhaus|basketball|baseball|bargains|" \
    "barefoot|barclays|barclaycard|barcelona|bar|bank|band|bananarepublic|banamex|baidu|baby|azure|axa|" \
    "aws|avianca|autos|auto|author|auspost|audio|audible|audi|auction|attorney|athleta|associates|asia|" \
    "asda|arte|art|arpa|army|archi|aramco|arab|aquarelle|apple|app|apartments|aol|anz|anquan|android|" \
    "analytics|amsterdam|amica|amfam|amex|americanfamily|americanexpress|alstom|alsace|ally|allstate|" \
    "allfinanz|alipay|alibaba|alfaromeo|akdn|airtel|airforce|airbus|aigo|aig|agency|agakhan|africa|afl|" \
    "afamilycompany|aetna|aero|aeg|adult|ads|adac|actor|active|aco|accountants|accountant|accenture|" \
    "academy|abudhabi|abogado|able|abc|abbvie|abbott|abb|abarth|aarp|aaa|onion" \
")(?=[^a-z0-9@+-]|$))"

#define TWUValidCCTLD \
"(?:(?:" \
    "한국|香港|澳門|新加坡|台灣|台湾|中國|中国|გე|ລາວ|ไทย|ලංකා|ഭാരതം|ಭಾರತ|భారత్|சிங்கப்பூர்|இலங்கை|இந்தியா|ଭାରତ|ભારત|ਭਾਰਤ|" \
    "ভাৰত|ভারত|বাংলা|भारोत|भारतम्|भारत|ڀارت|پاکستان|موريتانيا|مليسيا|مصر|قطر|فلسطين|عمان|عراق|سورية|سودان|" \
    "تونس|بھارت|بارت|ایران|امارات|المغرب|السعودية|الجزائر|البحرين|الاردن|հայ|қаз|укр|срб|рф|мон|мкд|ею|" \
    "бел|бг|ευ|ελ|zw|zm|za|yt|ye|ws|wf|vu|vn|vi|vg|ve|vc|va|uz|uy|us|um|uk|ug|ua|tz|tw|tv|tt|tr|tp|to|tn|" \
    "tm|tl|tk|tj|th|tg|tf|td|tc|sz|sy|sx|sv|su|st|ss|sr|so|sn|sm|sl|sk|sj|si|sh|sg|se|sd|sc|sb|sa|rw|ru|" \
    "rs|ro|re|qa|py|pw|pt|ps|pr|pn|pm|pl|pk|ph|pg|pf|pe|pa|om|nz|nu|nr|np|no|nl|ni|ng|nf|ne|nc|na|mz|my|" \
    "mx|mw|mv|mu|mt|ms|mr|mq|mp|mo|mn|mm|ml|mk|mh|mg|mf|me|md|mc|ma|ly|lv|lu|lt|ls|lr|lk|li|lc|lb|la|kz|" \
    "ky|kw|kr|kp|kn|km|ki|kh|kg|ke|jp|jo|jm|je|it|is|ir|iq|io|in|im|il|ie|id|hu|ht|hr|hn|hm|hk|gy|gw|gu|" \
    "gt|gs|gr|gq|gp|gn|gm|gl|gi|gh|gg|gf|ge|gd|gb|ga|fr|fo|fm|fk|fj|fi|eu|et|es|er|eh|eg|ee|ec|dz|do|dm|" \
    "dk|dj|de|cz|cy|cx|cw|cv|cu|cr|co|cn|cm|cl|ck|ci|ch|cg|cf|cd|cc|ca|bz|by|bw|bv|bt|bs|br|bq|bo|bn|bm|" \
    "bl|bj|bi|bh|bg|bf|be|bd|bb|ba|az|ax|aw|au|at|as|ar|aq|ao|an|am|al|ai|ag|af|ae|ad|ac" \
")(?=[^a-z0-9@+-]|$))"

#define TWUValidTCOURL                  "^https?://t\\.co/([a-z0-9]+)"

#define TWUValidURLPath \
"(?:" \
    "(?:" \
        TWUValidGeneralURLPathChars "*" \
        "(?:" TWUValidURLBalancedParens TWUValidGeneralURLPathChars "*)*" TWUValidURLPathEndingChars \
    ")" \
    "|" \
    "(?:" TWUValidGeneralURLPathChars "+/)" \
")"

// Transcribed from submodules/twitter-text/objc/lib/TwitterTextEmoji.h
// (TwitterTextEmojiPattern()), with every embedded Unicode literal converted to
// an explicit "\\u"/"\\U" ICU regex escape (see the file header comment above).
#define TwitterTextEmojiPatternUtf8 "(?:\\U0001f468\\U0001f3fb\\u200d\\U0001f91d\\u200d\\U0001f468[\\U0001f3fc-\\U0001f3ff]|\\U0001f468\\U0001f3fc\\u200d\\U0001f91d\\u200d\\U0001f468[\\U0001f3fb\\U0001f3fd-\\U0001f3ff]|\\U0001f468\\U0001f3fd\\u200d\\U0001f91d\\u200d\\U0001f468[\\U0001f3fb\\U0001f3fc\\U0001f3fe\\U0001f3ff]|\\U0001f468\\U0001f3fe\\u200d\\U0001f91d\\u200d\\U0001f468[\\U0001f3fb-\\U0001f3fd\\U0001f3ff]|\\U0001f468\\U0001f3ff\\u200d\\U0001f91d\\u200d\\U0001f468[\\U0001f3fb-\\U0001f3fe]|\\U0001f469\\U0001f3fb\\u200d\\U0001f91d\\u200d\\U0001f468[\\U0001f3fc-\\U0001f3ff]|\\U0001f469\\U0001f3fb\\u200d\\U0001f91d\\u200d\\U0001f469[\\U0001f3fc-\\U0001f3ff]|\\U0001f469\\U0001f3fc\\u200d\\U0001f91d\\u200d\\U0001f468[\\U0001f3fb\\U0001f3fd-\\U0001f3ff]|\\U0001f469\\U0001f3fc\\u200d\\U0001f91d\\u200d\\U0001f469[\\U0001f3fb\\U0001f3fd-\\U0001f3ff]|\\U0001f469\\U0001f3fd\\u200d\\U0001f91d\\u200d\\U0001f468[\\U0001f3fb\\U0001f3fc\\U0001f3fe\\U0001f3ff]|\\U0001f469\\U0001f3fd\\u200d\\U0001f91d\\u200d\\U0001f469[\\U0001f3fb\\U0001f3fc\\U0001f3fe\\U0001f3ff]|\\U0001f469\\U0001f3fe\\u200d\\U0001f91d\\u200d\\U0001f468[\\U0001f3fb-\\U0001f3fd\\U0001f3ff]|\\U0001f469\\U0001f3fe\\u200d\\U0001f91d\\u200d\\U0001f469[\\U0001f3fb-\\U0001f3fd\\U0001f3ff]|\\U0001f469\\U0001f3ff\\u200d\\U0001f91d\\u200d\\U0001f468[\\U0001f3fb-\\U0001f3fe]|\\U0001f469\\U0001f3ff\\u200d\\U0001f91d\\u200d\\U0001f469[\\U0001f3fb-\\U0001f3fe]|\\U0001f9d1\\U0001f3fb\\u200d\\U0001f91d\\u200d\\U0001f9d1[\\U0001f3fb-\\U0001f3ff]|\\U0001f9d1\\U0001f3fc\\u200d\\U0001f91d\\u200d\\U0001f9d1[\\U0001f3fb-\\U0001f3ff]|\\U0001f9d1\\U0001f3fd\\u200d\\U0001f91d\\u200d\\U0001f9d1[\\U0001f3fb-\\U0001f3ff]|\\U0001f9d1\\U0001f3fe\\u200d\\U0001f91d\\u200d\\U0001f9d1[\\U0001f3fb-\\U0001f3ff]|\\U0001f9d1\\U0001f3ff\\u200d\\U0001f91d\\u200d\\U0001f9d1[\\U0001f3fb-\\U0001f3ff]|\\U0001f9d1\\u200d\\U0001f91d\\u200d\\U0001f9d1|\\U0001f46b[\\U0001f3fb-\\U0001f3ff]|\\U0001f46c[\\U0001f3fb-\\U0001f3ff]|\\U0001f46d[\\U0001f3fb-\\U0001f3ff]|[\\U0001f46b-\\U0001f46d])|[\\U0001f468\\U0001f469\\U0001f9d1][\\U0001f3fb-\\U0001f3ff]?\\u200d(?:\\u2695\\ufe0f|\\u2696\\ufe0f|\\u2708\\ufe0f|[\\U0001f33e\\U0001f373\\U0001f393\\U0001f3a4\\U0001f3a8\\U0001f3eb\\U0001f3ed\\U0001f4bb\\U0001f4bc\\U0001f527\\U0001f52c\\U0001f680\\U0001f692\\U0001f9af-\\U0001f9b3\\U0001f9bc\\U0001f9bd])|[\\u26f9\\U0001f3cb\\U0001f3cc\\U0001f574\\U0001f575]([\\ufe0f\\U0001f3fb-\\U0001f3ff]\\u200d[\\u2640\\u2642]\\ufe0f)|[\\U0001f3c3\\U0001f3c4\\U0001f3ca\\U0001f46e\\U0001f471\\U0001f473\\U0001f477\\U0001f481\\U0001f482\\U0001f486\\U0001f487\\U0001f645-\\U0001f647\\U0001f64b\\U0001f64d\\U0001f64e\\U0001f6a3\\U0001f6b4-\\U0001f6b6\\U0001f926\\U0001f935\\U0001f937-\\U0001f939\\U0001f93d\\U0001f93e\\U0001f9b8\\U0001f9b9\\U0001f9cd-\\U0001f9cf\\U0001f9d6-\\U0001f9dd][\\U0001f3fb-\\U0001f3ff]?\\u200d[\\u2640\\u2642]\\ufe0f|(?:\\U0001f468\\u200d\\u2764\\ufe0f\\u200d\\U0001f48b\\u200d\\U0001f468|\\U0001f469\\u200d\\u2764\\ufe0f\\u200d\\U0001f48b\\u200d[\\U0001f468\\U0001f469]|\\U0001f468\\u200d\\U0001f468\\u200d\\U0001f466\\u200d\\U0001f466|\\U0001f468\\u200d\\U0001f468\\u200d\\U0001f467\\u200d[\\U0001f466\\U0001f467]|\\U0001f468\\u200d\\U0001f469\\u200d\\U0001f466\\u200d\\U0001f466|\\U0001f468\\u200d\\U0001f469\\u200d\\U0001f467\\u200d[\\U0001f466\\U0001f467]|\\U0001f469\\u200d\\U0001f469\\u200d\\U0001f466\\u200d\\U0001f466|\\U0001f469\\u200d\\U0001f469\\u200d\\U0001f467\\u200d[\\U0001f466\\U0001f467]|\\U0001f468\\u200d\\u2764\\ufe0f\\u200d\\U0001f468|\\U0001f469\\u200d\\u2764\\ufe0f\\u200d[\\U0001f468\\U0001f469]|\\U0001f3f3\\ufe0f\\u200d\\u26a7\\ufe0f|\\U0001f468\\u200d\\U0001f466\\u200d\\U0001f466|\\U0001f468\\u200d\\U0001f467\\u200d[\\U0001f466\\U0001f467]|\\U0001f468\\u200d\\U0001f468\\u200d[\\U0001f466\\U0001f467]|\\U0001f468\\u200d\\U0001f469\\u200d[\\U0001f466\\U0001f467]|\\U0001f469\\u200d\\U0001f466\\u200d\\U0001f466|\\U0001f469\\u200d\\U0001f467\\u200d[\\U0001f466\\U0001f467]|\\U0001f469\\u200d\\U0001f469\\u200d[\\U0001f466\\U0001f467]|\\U0001f3f3\\ufe0f\\u200d\\U0001f308|\\U0001f3f4\\u200d\\u2620\\ufe0f|\\U0001f46f\\u200d\\u2640\\ufe0f|\\U0001f46f\\u200d\\u2642\\ufe0f|\\U0001f93c\\u200d\\u2640\\ufe0f|\\U0001f93c\\u200d\\u2642\\ufe0f|\\U0001f9de\\u200d\\u2640\\ufe0f|\\U0001f9de\\u200d\\u2642\\ufe0f|\\U0001f9df\\u200d\\u2640\\ufe0f|\\U0001f9df\\u200d\\u2642\\ufe0f|\\U0001f415\\u200d\\U0001f9ba|\\U0001f441\\u200d\\U0001f5e8|\\U0001f468\\u200d[\\U0001f466\\U0001f467]|\\U0001f469\\u200d[\\U0001f466\\U0001f467])|[#*0-9]\\ufe0f?\\u20e3|(?:[\\u00A9\\u00AE\\u2122\\u265f]\\ufe0f)|[\\u203c\\u2049\\u2139\\u2194-\\u2199\\u21a9\\u21aa\\u231a\\u231b\\u2328\\u23cf\\u23ed-\\u23ef\\u23f1\\u23f2\\u23f8-\\u23fa\\u24c2\\u25aa\\u25ab\\u25b6\\u25c0\\u25fb-\\u25fe\\u2600-\\u2604\\u260e\\u2611\\u2614\\u2615\\u2618\\u2620\\u2622\\u2623\\u2626\\u262a\\u262e\\u262f\\u2638-\\u263a\\u2640\\u2642\\u2648-\\u2653\\u2660\\u2663\\u2665\\u2666\\u2668\\u267b\\u267f\\u2692-\\u2697\\u2699\\u269b\\u269c\\u26a0\\u26a1\\u26a7\\u26aa\\u26ab\\u26b0\\u26b1\\u26bd\\u26be\\u26c4\\u26c5\\u26c8\\u26cf\\u26d1\\u26d3\\u26d4\\u26e9\\u26ea\\u26f0-\\u26f5\\u26f8\\u26fa\\u26fd\\u2702\\u2708\\u2709\\u270f\\u2712\\u2714\\u2716\\u271d\\u2721\\u2733\\u2734\\u2744\\u2747\\u2757\\u2763\\u2764\\u27a1\\u2934\\u2935\\u2b05-\\u2b07\\u2b1b\\u2b1c\\u2b50\\u2b55\\u3030\\u303d\\u3297\\u3299\\U0001f004\\U0001f170\\U0001f171\\U0001f17e\\U0001f17f\\U0001f202\\U0001f21a\\U0001f22f\\U0001f237\\U0001f321\\U0001f324-\\U0001f32c\\U0001f336\\U0001f37d\\U0001f396\\U0001f397\\U0001f399-\\U0001f39b\\U0001f39e\\U0001f39f\\U0001f3cd\\U0001f3ce\\U0001f3d4-\\U0001f3df\\U0001f3f3\\U0001f3f5\\U0001f3f7\\U0001f43f\\U0001f441\\U0001f4fd\\U0001f549\\U0001f54a\\U0001f56f\\U0001f570\\U0001f573\\U0001f576-\\U0001f579\\U0001f587\\U0001f58a-\\U0001f58d\\U0001f5a5\\U0001f5a8\\U0001f5b1\\U0001f5b2\\U0001f5bc\\U0001f5c2-\\U0001f5c4\\U0001f5d1-\\U0001f5d3\\U0001f5dc-\\U0001f5de\\U0001f5e1\\U0001f5e3\\U0001f5e8\\U0001f5ef\\U0001f5f3\\U0001f5fa\\U0001f6cb\\U0001f6cd-\\U0001f6cf\\U0001f6e0-\\U0001f6e5\\U0001f6e9\\U0001f6f0\\U0001f6f3](?:\\ufe0f|(?!\\ufe0e))|(?:[\\u261d\\u26f7\\u26f9\\u270c\\u270d\\U0001f3cb\\U0001f3cc\\U0001f574\\U0001f575\\U0001f590](?:\\ufe0f|(?!\\ufe0e))|[\\u270a\\u270b\\U0001f385\\U0001f3c2-\\U0001f3c4\\U0001f3c7\\U0001f3ca\\U0001f442\\U0001f443\\U0001f446-\\U0001f450\\U0001f466-\\U0001f469\\U0001f46e\\U0001f470-\\U0001f478\\U0001f47c\\U0001f481-\\U0001f483\\U0001f485-\\U0001f487\\U0001f4aa\\U0001f57a\\U0001f595\\U0001f596\\U0001f645-\\U0001f647\\U0001f64b-\\U0001f64f\\U0001f6a3\\U0001f6b4-\\U0001f6b6\\U0001f6c0\\U0001f6cc\\U0001f90f\\U0001f918-\\U0001f91c\\U0001f91e\\U0001f91f\\U0001f926\\U0001f930-\\U0001f939\\U0001f93d\\U0001f93e\\U0001f9b5\\U0001f9b6\\U0001f9b8\\U0001f9b9\\U0001f9bb\\U0001f9cd-\\U0001f9cf\\U0001f9d1-\\U0001f9dd])[\\U0001f3fb-\\U0001f3ff]?|(?:\\U0001f3f4\\U000e0067\\U000e0062\\U000e0065\\U000e006e\\U000e0067\\U000e007f|\\U0001f3f4\\U000e0067\\U000e0062\\U000e0073\\U000e0063\\U000e0074\\U000e007f|\\U0001f3f4\\U000e0067\\U000e0062\\U000e0077\\U000e006c\\U000e0073\\U000e007f|\\U0001f1e6[\\U0001f1e8-\\U0001f1ec\\U0001f1ee\\U0001f1f1\\U0001f1f2\\U0001f1f4\\U0001f1f6-\\U0001f1fa\\U0001f1fc\\U0001f1fd\\U0001f1ff]|\\U0001f1e7[\\U0001f1e6\\U0001f1e7\\U0001f1e9-\\U0001f1ef\\U0001f1f1-\\U0001f1f4\\U0001f1f6-\\U0001f1f9\\U0001f1fb\\U0001f1fc\\U0001f1fe\\U0001f1ff]|\\U0001f1e8[\\U0001f1e6\\U0001f1e8\\U0001f1e9\\U0001f1eb-\\U0001f1ee\\U0001f1f0-\\U0001f1f5\\U0001f1f7\\U0001f1fa-\\U0001f1ff]|\\U0001f1e9[\\U0001f1ea\\U0001f1ec\\U0001f1ef\\U0001f1f0\\U0001f1f2\\U0001f1f4\\U0001f1ff]|\\U0001f1ea[\\U0001f1e6\\U0001f1e8\\U0001f1ea\\U0001f1ec\\U0001f1ed\\U0001f1f7-\\U0001f1fa]|\\U0001f1eb[\\U0001f1ee-\\U0001f1f0\\U0001f1f2\\U0001f1f4\\U0001f1f7]|\\U0001f1ec[\\U0001f1e6\\U0001f1e7\\U0001f1e9-\\U0001f1ee\\U0001f1f1-\\U0001f1f3\\U0001f1f5-\\U0001f1fa\\U0001f1fc\\U0001f1fe]|\\U0001f1ed[\\U0001f1f0\\U0001f1f2\\U0001f1f3\\U0001f1f7\\U0001f1f9\\U0001f1fa]|\\U0001f1ee[\\U0001f1e8-\\U0001f1ea\\U0001f1f1-\\U0001f1f4\\U0001f1f6-\\U0001f1f9]|\\U0001f1ef[\\U0001f1ea\\U0001f1f2\\U0001f1f4\\U0001f1f5]|\\U0001f1f0[\\U0001f1ea\\U0001f1ec-\\U0001f1ee\\U0001f1f2\\U0001f1f3\\U0001f1f5\\U0001f1f7\\U0001f1fc\\U0001f1fe\\U0001f1ff]|\\U0001f1f1[\\U0001f1e6-\\U0001f1e8\\U0001f1ee\\U0001f1f0\\U0001f1f7-\\U0001f1fb\\U0001f1fe]|\\U0001f1f2[\\U0001f1e6\\U0001f1e8-\\U0001f1ed\\U0001f1f0-\\U0001f1ff]|\\U0001f1f3[\\U0001f1e6\\U0001f1e8\\U0001f1ea-\\U0001f1ec\\U0001f1ee\\U0001f1f1\\U0001f1f4\\U0001f1f5\\U0001f1f7\\U0001f1fa\\U0001f1ff]|\\U0001f1f4\\U0001f1f2|\\U0001f1f5[\\U0001f1e6\\U0001f1ea-\\U0001f1ed\\U0001f1f0-\\U0001f1f3\\U0001f1f7-\\U0001f1f9\\U0001f1fc\\U0001f1fe]|\\U0001f1f6\\U0001f1e6|\\U0001f1f7[\\U0001f1ea\\U0001f1f4\\U0001f1f8\\U0001f1fa\\U0001f1fc]|\\U0001f1f8[\\U0001f1e6-\\U0001f1ea\\U0001f1ec-\\U0001f1f4\\U0001f1f7-\\U0001f1f9\\U0001f1fb\\U0001f1fd-\\U0001f1ff]|\\U0001f1f9[\\U0001f1e6\\U0001f1e8\\U0001f1e9\\U0001f1eb-\\U0001f1ed\\U0001f1ef-\\U0001f1f4\\U0001f1f7\\U0001f1f9\\U0001f1fb\\U0001f1fc\\U0001f1ff]|\\U0001f1fa[\\U0001f1e6\\U0001f1ec\\U0001f1f2\\U0001f1f3\\U0001f1f8\\U0001f1fe\\U0001f1ff]|\\U0001f1fb[\\U0001f1e6\\U0001f1e8\\U0001f1ea\\U0001f1ec\\U0001f1ee\\U0001f1f3\\U0001f1fa]|\\U0001f1fc[\\U0001f1eb\\U0001f1f8]|\\U0001f1fd\\U0001f1f0|\\U0001f1fe[\\U0001f1ea\\U0001f1f9]|\\U0001f1ff[\\U0001f1e6\\U0001f1f2\\U0001f1fc]|[\\u23e9-\\u23ec\\u23f0\\u23f3\\u267e\\u26ce\\u2705\\u2728\\u274c\\u274e\\u2753-\\u2755\\u2795-\\u2797\\u27b0\\u27bf\\ue50a\\U0001f0cf\\U0001f18e\\U0001f191-\\U0001f19a\\U0001f1e6-\\U0001f1ff\\U0001f201\\U0001f232-\\U0001f236\\U0001f238-\\U0001f23a\\U0001f250\\U0001f251\\U0001f300-\\U0001f320\\U0001f32d-\\U0001f335\\U0001f337-\\U0001f37c\\U0001f37e-\\U0001f384\\U0001f386-\\U0001f393\\U0001f3a0-\\U0001f3c1\\U0001f3c5\\U0001f3c6\\U0001f3c8\\U0001f3c9\\U0001f3cf-\\U0001f3d3\\U0001f3e0-\\U0001f3f0\\U0001f3f4\\U0001f3f8-\\U0001f43e\\U0001f440\\U0001f444\\U0001f445\\U0001f451-\\U0001f465\\U0001f46a\\U0001f46f\\U0001f479-\\U0001f47b\\U0001f47d-\\U0001f480\\U0001f484\\U0001f488-\\U0001f4a9\\U0001f4ab-\\U0001f4fc\\U0001f4ff-\\U0001f53d\\U0001f54b-\\U0001f54e\\U0001f550-\\U0001f567\\U0001f5a4\\U0001f5fb-\\U0001f644\\U0001f648-\\U0001f64a\\U0001f680-\\U0001f6a2\\U0001f6a4-\\U0001f6b3\\U0001f6b7-\\U0001f6bf\\U0001f6c1-\\U0001f6c5\\U0001f6d0-\\U0001f6d2\\U0001f6d5\\U0001f6eb\\U0001f6ec\\U0001f6f4-\\U0001f6fa\\U0001f7e0-\\U0001f7eb\\U0001f90d\\U0001f90e\\U0001f910-\\U0001f917\\U0001f91d\\U0001f920-\\U0001f925\\U0001f927-\\U0001f92f\\U0001f93a\\U0001f93c\\U0001f93f-\\U0001f945\\U0001f947-\\U0001f971\\U0001f973-\\U0001f976\\U0001f97a-\\U0001f9a2\\U0001f9a5-\\U0001f9aa\\U0001f9ae-\\U0001f9b4\\U0001f9b7\\U0001f9ba\\U0001f9bc-\\U0001f9ca\\U0001f9d0\\U0001f9de-\\U0001f9ff\\U0001fa70-\\U0001fa73\\U0001fa78-\\U0001fa7a\\U0001fa80-\\U0001fa82\\U0001fa90-\\U0001fa95])|\\ufe0f"

// clang-format on

namespace TwitterText {

namespace {

constexpr int32_t kMaxURLLength = 4096;
constexpr int32_t kMaxTCOSlugLength = 40;
constexpr int32_t kURLProtocolLength = 8; // length of "https://"

const UChar* AsUChar(const std::u16string& s) {
    return reinterpret_cast<const UChar*>(s.data());
}

UChar* AsUChar(std::u16string& s) {
    return reinterpret_cast<UChar*>(s.data());
}

std::u16string Utf16FromUtf8(const char* utf8, int32_t utf8Length) {
    UErrorCode status = U_ZERO_ERROR;
    int32_t requiredLength = 0;
    u_strFromUTF8(nullptr, 0, &requiredLength, utf8, utf8Length, &status);
    if (status != U_BUFFER_OVERFLOW_ERROR && U_FAILURE(status)) {
        return std::u16string();
    }
    status = U_ZERO_ERROR;
    std::u16string result(static_cast<size_t>(requiredLength), u'\0');
    u_strFromUTF8(AsUChar(result), requiredLength, nullptr, utf8, utf8Length, &status);
    if (U_FAILURE(status)) {
        return std::u16string();
    }
    return result;
}

std::u16string Utf16FromUtf8(const char* utf8) {
    return Utf16FromUtf8(utf8, -1);
}

// A regex "template": compiled once, then cheaply uregex_clone()'d into an
// independent, thread-safe-to-use matcher for each search. This mirrors the
// icu::RegexPattern::matcher() idiom from the (Apple-only) C++ API.
class CompiledRegex {
public:
    CompiledRegex(const char* patternUtf8, uint32_t flags) {
        std::u16string pattern = Utf16FromUtf8(patternUtf8);
        UErrorCode status = U_ZERO_ERROR;
        template_ = uregex_open(AsUChar(pattern), static_cast<int32_t>(pattern.length()), flags, nullptr, &status);
        // A failure here means one of the transcribed patterns above has a
        // syntax error; there is nothing sensible to do at runtime other than
        // to fail loudly.
        if (U_FAILURE(status) || template_ == nullptr) {
            std::terminate();
        }
    }

    CompiledRegex(const CompiledRegex&) = delete;
    CompiledRegex& operator=(const CompiledRegex&) = delete;

    ~CompiledRegex() {
        uregex_close(template_);
    }

    URegularExpression* NewMatcher() const {
        UErrorCode status = U_ZERO_ERROR;
        URegularExpression* clone = uregex_clone(template_, &status);
        if (U_FAILURE(status)) {
            return nullptr;
        }
        return clone;
    }

private:
    URegularExpression* template_ = nullptr;
};

struct MatcherDeleter {
    void operator()(URegularExpression* re) const {
        if (re != nullptr) {
            uregex_close(re);
        }
    }
};
using MatcherPtr = std::unique_ptr<URegularExpression, MatcherDeleter>;

MatcherPtr NewMatcherFor(const CompiledRegex& regex) {
    return MatcherPtr(regex.NewMatcher());
}

#define TWITTERTEXT_LAZY_PATTERN(FUNC_NAME, PATTERN, ...)              \
    const CompiledRegex& FUNC_NAME() {                                 \
        static const CompiledRegex regex(PATTERN, ##__VA_ARGS__);      \
        return regex;                                                  \
    }

TWITTERTEXT_LAZY_PATTERN(ValidUrlPattern, TWUValidURLPatternString, UREGEX_CASE_INSENSITIVE)
TWITTERTEXT_LAZY_PATTERN(ValidTcoUrlPattern, TWUValidTCOURL, UREGEX_CASE_INSENSITIVE)
TWITTERTEXT_LAZY_PATTERN(ValidHashtagPattern, TWUValidHashtag, UREGEX_CASE_INSENSITIVE)
TWITTERTEXT_LAZY_PATTERN(EndHashtagPattern, TWUEndHashTagMatch, UREGEX_CASE_INSENSITIVE)
TWITTERTEXT_LAZY_PATTERN(ValidSymbolPattern, TWUValidSymbol, UREGEX_CASE_INSENSITIVE)
TWITTERTEXT_LAZY_PATTERN(ValidMentionOrListPattern, TWUValidMentionOrList, UREGEX_CASE_INSENSITIVE)
TWITTERTEXT_LAZY_PATTERN(ValidReplyPattern, TWUValidReply, UREGEX_CASE_INSENSITIVE)
TWITTERTEXT_LAZY_PATTERN(EndMentionPattern, TWUEndMentionMatch, UREGEX_CASE_INSENSITIVE)
TWITTERTEXT_LAZY_PATTERN(EmojiPattern, TwitterTextEmojiPatternUtf8, 0)

#undef TWITTERTEXT_LAZY_PATTERN

// Mirrors NSMatchingWithoutAnchoringBounds + a [position, text.length()) search
// range: ^ / \A only match the true start of `text`, not `position`, and
// lookaround is opaque to anything outside the region (twitter-text never needs
// to look behind `position` since matches are only ever searched forward).
bool FindFirstMatchFrom(URegularExpression* matcher, const std::u16string& text, int32_t position) {
    UErrorCode status = U_ZERO_ERROR;
    uregex_setText(matcher, AsUChar(const_cast<std::u16string&>(text)), static_cast<int32_t>(text.length()), &status);
    uregex_setRegion(matcher, position, static_cast<int32_t>(text.length()), &status);
    uregex_useAnchoringBounds(matcher, false, &status);
    if (U_FAILURE(status)) {
        return false;
    }
    UBool found = uregex_findNext(matcher, &status);
    return U_SUCCESS(status) && found;
}

// Mirrors NSRegularExpression's default options (anchoring bounds ON, opaque):
// used for the "does the text right after this match forbid it" lookahead-style
// checks (TWUEndHashTagMatch / TWUEndMentionMatch), where \A must bind to
// `position`, not the true start of `text`.
bool RegionContainsMatch(URegularExpression* matcher, const std::u16string& text, int32_t position) {
    UErrorCode status = U_ZERO_ERROR;
    uregex_setText(matcher, AsUChar(const_cast<std::u16string&>(text)), static_cast<int32_t>(text.length()), &status);
    uregex_setRegion(matcher, position, static_cast<int32_t>(text.length()), &status);
    // Default anchoring bounds (true) intentionally left as-is.
    if (U_FAILURE(status)) {
        return false;
    }
    UBool found = uregex_findNext(matcher, &status);
    return U_SUCCESS(status) && found;
}

// A single, unrestricted search of the whole string from the start, used
// where twitter-text does not slide a scan position (TCO slug match, reply
// match).
bool FindInWholeString(URegularExpression* matcher, const std::u16string& text) {
    UErrorCode status = U_ZERO_ERROR;
    uregex_setText(matcher, AsUChar(const_cast<std::u16string&>(text)), static_cast<int32_t>(text.length()), &status);
    if (U_FAILURE(status)) {
        return false;
    }
    UBool found = uregex_find(matcher, 0, &status);
    return U_SUCCESS(status) && found;
}

int32_t GroupCount(URegularExpression* matcher) {
    UErrorCode status = U_ZERO_ERROR;
    int32_t count = uregex_groupCount(matcher, &status);
    return U_SUCCESS(status) ? count : -1;
}

bool GroupRange(URegularExpression* matcher, int32_t group, int32_t& outStart, int32_t& outEnd) {
    UErrorCode status = U_ZERO_ERROR;
    outStart = uregex_start(matcher, group, &status);
    if (U_FAILURE(status) || outStart < 0) {
        return false;
    }
    UErrorCode endStatus = U_ZERO_ERROR;
    outEnd = uregex_end(matcher, group, &endStatus);
    return U_SUCCESS(endStatus);
}

std::u16string SubStr(const std::u16string& text, int32_t start, int32_t end) {
    return text.substr(static_cast<size_t>(start), static_cast<size_t>(end - start));
}

bool RangesIntersect(int32_t aStart, int32_t aEnd, int32_t bStart, int32_t bEnd) {
    return aStart < bEnd && bStart < aEnd;
}

bool OverlapsAny(const std::vector<Entity>& entities, int32_t start, int32_t end) {
    for (const Entity& entity : entities) {
        if (RangesIntersect(entity.start, entity.end, start, end)) {
            return true;
        }
    }
    return false;
}

// Punycode/IDNA validity + length check, mirroring TwitterText.m's
// isValidHostAndLength: (which uses the bundled IFUnicodeURL library on iOS) and
// Extractor.java's isValidHostAndLength (which uses java.net.IDN.toASCII).
bool IsValidHostAndLength(int32_t urlLength, bool hasProtocol, const std::u16string& host) {
    if (host.empty()) {
        return false;
    }

    int32_t originalHostLength = static_cast<int32_t>(host.length());
    int32_t updatedHostLength = originalHostLength;

    UErrorCode openStatus = U_ZERO_ERROR;
    static UIDNA* idna = uidna_openUTS46(0, &openStatus);
    if (idna != nullptr && U_SUCCESS(openStatus)) {
        UErrorCode convertStatus = U_ZERO_ERROR;
        UIDNAInfo info = UIDNA_INFO_INITIALIZER;
        // Preflight to discover the required buffer size.
        int32_t needed = uidna_nameToASCII(idna, AsUChar(const_cast<std::u16string&>(host)), originalHostLength, nullptr, 0, &info, &convertStatus);
        if (convertStatus == U_BUFFER_OVERFLOW_ERROR || U_SUCCESS(convertStatus)) {
            std::u16string processed(static_cast<size_t>(std::max(needed, 0)), u'\0');
            UErrorCode finalStatus = U_ZERO_ERROR;
            UIDNAInfo finalInfo = UIDNA_INFO_INITIALIZER;
            uidna_nameToASCII(idna, AsUChar(const_cast<std::u16string&>(host)), originalHostLength, AsUChar(processed), needed, &finalInfo, &finalStatus);
            if (U_SUCCESS(finalStatus)) {
                if (finalInfo.errors != 0) {
                    if ((finalInfo.errors & UIDNA_ERROR_LABEL_TOO_LONG) != 0) {
                        // Mirrors IFUnicodeURLConvertErrorInvalidDNSLength: a
                        // label over 63 octets is always invalid, matching
                        // NSURL's refusal to build a URL from such a host.
                        return false;
                    }
                    // Other errors (e.g. disallowed / non-LDH characters) are
                    // tolerated -- fall back to treating the host as already
                    // valid, matching the Objective-C fallback to NSURL
                    // URLWithString:.
                } else {
                    updatedHostLength = needed;
                }
            }
        }
    }

    if (updatedHostLength == 0) {
        return false;
    }
    if (updatedHostLength > originalHostLength) {
        urlLength += (updatedHostLength - originalHostLength);
    }

    int32_t urlLengthWithProtocol = urlLength;
    if (!hasProtocol) {
        urlLengthWithProtocol += kURLProtocolLength;
    }
    return urlLengthWithProtocol <= kMaxURLLength;
}

} // namespace

std::vector<Entity> UrlsInText(const std::u16string& text) {
    std::vector<Entity> results;
    if (text.empty()) {
        return results;
    }

    int32_t len = static_cast<int32_t>(text.length());
    int32_t position = 0;

    MatcherPtr matcher = NewMatcherFor(ValidUrlPattern());
    MatcherPtr tcoMatcher = NewMatcherFor(ValidTcoUrlPattern());
    if (!matcher || !tcoMatcher) {
        return results;
    }

    while (position < len) {
        if (!FindFirstMatchFrom(matcher.get(), text, position)) {
            break;
        }

        UErrorCode rangeStatus = U_ZERO_ERROR;
        int32_t matchEnd = uregex_end(matcher.get(), 0, &rangeStatus);
        if (U_FAILURE(rangeStatus)) {
            break;
        }
        // Default to continuing right after this match if we bail out below,
        // mirroring the Objective-C "continue processing after the end of
        // this invalid result" behavior.
        position = matchEnd;

        if (GroupCount(matcher.get()) < TWUValidURLGroupQueryString) {
            continue;
        }

        int32_t urlStart = 0, urlEnd = 0;
        bool hasUrl = GroupRange(matcher.get(), TWUValidURLGroupURL, urlStart, urlEnd);
        int32_t precedingStart = 0, precedingEnd = 0;
        bool hasPreceding = GroupRange(matcher.get(), TWUValidURLGroupPreceding, precedingStart, precedingEnd);
        int32_t protocolStart = 0, protocolEnd = 0;
        bool hasProtocol = GroupRange(matcher.get(), TWUValidURLGroupProtocol, protocolStart, protocolEnd);
        int32_t domainStart = 0, domainEnd = 0;
        bool hasDomain = GroupRange(matcher.get(), TWUValidURLGroupDomain, domainStart, domainEnd);

        if (!hasProtocol && hasPreceding) {
            std::u16string preceding = SubStr(text, precedingStart, precedingEnd);
            if (!preceding.empty()) {
                char16_t last = preceding.back();
                if (last == u'-' || last == u'_' || last == u'.' || last == u'/') {
                    continue;
                }
            }
        }

        if (!hasUrl) {
            continue;
        }

        std::u16string url = SubStr(text, urlStart, urlEnd);
        std::u16string host = hasDomain ? SubStr(text, domainStart, domainEnd) : std::u16string();

        int32_t start = urlStart;
        int32_t end = urlEnd;

        if (FindInWholeString(tcoMatcher.get(), url) && GroupCount(tcoMatcher.get()) >= 1) {
            UErrorCode tcoRangeStatus = U_ZERO_ERROR;
            int32_t tcoStart = uregex_start(tcoMatcher.get(), 0, &tcoRangeStatus);
            int32_t tcoEnd = uregex_end(tcoMatcher.get(), 0, &tcoRangeStatus);
            int32_t slugStart = 0, slugEnd = 0;
            bool hasSlug = GroupRange(tcoMatcher.get(), 1, slugStart, slugEnd);
            if (U_SUCCESS(tcoRangeStatus) && tcoStart >= 0 && hasSlug) {
                int32_t slugLength = slugEnd - slugStart;
                if (slugLength > kMaxTCOSlugLength) {
                    continue;
                }
                url = SubStr(url, tcoStart, tcoEnd);
                end = start + static_cast<int32_t>(url.length());
            }
        }

        std::u16string protocolText = hasProtocol ? SubStr(text, protocolStart, protocolEnd) : std::u16string();
        if (IsValidHostAndLength(static_cast<int32_t>(url.length()), !protocolText.empty(), host)) {
            results.push_back(Entity{EntityType::Url, start, end});
            position = end;
        }
    }

    return results;
}

std::vector<Entity> HashtagsInText(const std::u16string& text, bool checkingUrlOverlap) {
    std::vector<Entity> results;
    if (text.empty()) {
        return results;
    }

    std::vector<Entity> urls;
    if (checkingUrlOverlap) {
        urls = UrlsInText(text);
    }

    int32_t len = static_cast<int32_t>(text.length());
    int32_t position = 0;

    MatcherPtr matcher = NewMatcherFor(ValidHashtagPattern());
    MatcherPtr endMatcher = NewMatcherFor(EndHashtagPattern());
    if (!matcher || !endMatcher) {
        return results;
    }

    while (position < len) {
        if (!FindFirstMatchFrom(matcher.get(), text, position) || GroupCount(matcher.get()) < 1) {
            break;
        }

        UErrorCode rangeStatus = U_ZERO_ERROR;
        int32_t matchEnd = uregex_end(matcher.get(), 0, &rangeStatus);
        int32_t hashtagStart = 0, hashtagEnd = 0;
        bool hasHashtag = GroupRange(matcher.get(), 1, hashtagStart, hashtagEnd);
        if (U_FAILURE(rangeStatus) || !hasHashtag) {
            break;
        }

        bool matchOk = !OverlapsAny(urls, hashtagStart, hashtagEnd);

        if (matchOk) {
            int32_t afterStart = hashtagEnd;
            if (afterStart < len && RegionContainsMatch(endMatcher.get(), text, afterStart)) {
                matchOk = false;
            }
        }

        if (matchOk) {
            results.push_back(Entity{EntityType::Hashtag, hashtagStart, hashtagEnd});
        }

        position = matchEnd;
    }

    return results;
}

std::vector<Entity> SymbolsInText(const std::u16string& text, bool checkingUrlOverlap) {
    std::vector<Entity> results;
    if (text.empty()) {
        return results;
    }

    std::vector<Entity> urls;
    if (checkingUrlOverlap) {
        urls = UrlsInText(text);
    }

    int32_t len = static_cast<int32_t>(text.length());
    int32_t position = 0;

    MatcherPtr matcher = NewMatcherFor(ValidSymbolPattern());
    if (!matcher) {
        return results;
    }

    while (position < len) {
        if (!FindFirstMatchFrom(matcher.get(), text, position) || GroupCount(matcher.get()) < 1) {
            break;
        }

        UErrorCode rangeStatus = U_ZERO_ERROR;
        int32_t matchEnd = uregex_end(matcher.get(), 0, &rangeStatus);
        int32_t symbolStart = 0, symbolEnd = 0;
        bool hasSymbol = GroupRange(matcher.get(), 1, symbolStart, symbolEnd);
        if (U_FAILURE(rangeStatus) || !hasSymbol) {
            break;
        }

        if (!OverlapsAny(urls, symbolStart, symbolEnd)) {
            results.push_back(Entity{EntityType::Symbol, symbolStart, symbolEnd});
        }

        position = matchEnd;
    }

    return results;
}

std::vector<Entity> MentionsOrListsInText(const std::u16string& text) {
    std::vector<Entity> results;
    if (text.empty()) {
        return results;
    }

    int32_t len = static_cast<int32_t>(text.length());
    int32_t position = 0;

    MatcherPtr matcher = NewMatcherFor(ValidMentionOrListPattern());
    MatcherPtr endMatcher = NewMatcherFor(EndMentionPattern());
    if (!matcher || !endMatcher) {
        return results;
    }

    while (position < len) {
        if (!FindFirstMatchFrom(matcher.get(), text, position) || GroupCount(matcher.get()) < 4) {
            break;
        }

        UErrorCode rangeStatus = U_ZERO_ERROR;
        int32_t matchEnd = uregex_end(matcher.get(), 0, &rangeStatus);
        if (U_FAILURE(rangeStatus)) {
            break;
        }

        bool endMentionFound = RegionContainsMatch(endMatcher.get(), text, matchEnd);

        if (!endMentionFound) {
            int32_t atSignStart = 0, atSignEnd = 0;
            bool hasAtSign = GroupRange(matcher.get(), 2, atSignStart, atSignEnd);
            int32_t screenNameStart = 0, screenNameEnd = 0;
            GroupRange(matcher.get(), 3, screenNameStart, screenNameEnd);
            int32_t listNameStart = 0, listNameEnd = 0;
            bool hasListName = GroupRange(matcher.get(), 4, listNameStart, listNameEnd);

            if (hasAtSign) {
                if (!hasListName) {
                    results.push_back(Entity{EntityType::ScreenName, atSignStart, screenNameEnd});
                } else {
                    results.push_back(Entity{EntityType::ListName, atSignStart, listNameEnd});
                }
            }
            position = matchEnd;
        } else {
            // Avoid matching the second username in @username@username.
            position = matchEnd + 1;
        }
    }

    return results;
}

std::vector<Entity> MentionedScreenNamesInText(const std::u16string& text) {
    std::vector<Entity> results;
    for (const Entity& entity : MentionsOrListsInText(text)) {
        if (entity.type == EntityType::ScreenName) {
            results.push_back(entity);
        }
    }
    return results;
}

std::optional<Entity> RepliedScreenNameInText(const std::u16string& text) {
    if (text.empty()) {
        return std::nullopt;
    }

    int32_t len = static_cast<int32_t>(text.length());

    MatcherPtr matcher = NewMatcherFor(ValidReplyPattern());
    if (!matcher) {
        return std::nullopt;
    }

    if (!FindInWholeString(matcher.get(), text) || GroupCount(matcher.get()) < 1) {
        return std::nullopt;
    }

    int32_t replyStart = 0, replyEnd = 0;
    if (!GroupRange(matcher.get(), 1, replyStart, replyEnd)) {
        return std::nullopt;
    }

    MatcherPtr endMatcher = NewMatcherFor(EndMentionPattern());
    if (endMatcher && replyEnd < len && RegionContainsMatch(endMatcher.get(), text, replyEnd)) {
        return std::nullopt;
    }

    return Entity{EntityType::ScreenName, replyStart, replyEnd};
}

std::optional<std::u16string> ListSlugForEntity(const std::u16string& text, const Entity& entity) {
    if (entity.type != EntityType::ListName) {
        return std::nullopt;
    }
    std::u16string value = SubStr(text, entity.start, entity.end);
    size_t slashIndex = value.find(u'/');
    if (slashIndex == std::u16string::npos) {
        return std::nullopt;
    }
    return value.substr(slashIndex);
}

std::vector<Entity> EntitiesInText(const std::u16string& text) {
    std::vector<Entity> results;
    if (text.empty()) {
        return results;
    }

    std::vector<Entity> urls = UrlsInText(text);
    results.insert(results.end(), urls.begin(), urls.end());

    std::vector<Entity> hashtags = HashtagsInText(text, /*checkingUrlOverlap=*/false);
    for (const Entity& hashtag : hashtags) {
        if (!OverlapsAny(urls, hashtag.start, hashtag.end)) {
            results.push_back(hashtag);
        }
    }

    std::vector<Entity> symbols = SymbolsInText(text, /*checkingUrlOverlap=*/false);
    for (const Entity& symbol : symbols) {
        if (!OverlapsAny(urls, symbol.start, symbol.end)) {
            results.push_back(symbol);
        }
    }

    std::vector<Entity> mentionsOrLists = MentionsOrListsInText(text);
    for (const Entity& entity : mentionsOrLists) {
        bool found = false;
        for (const Entity& existing : results) {
            if (RangesIntersect(existing.start, existing.end, entity.start, entity.end)) {
                found = true;
                break;
            }
        }
        if (!found) {
            results.push_back(entity);
        }
    }

    std::sort(results.begin(), results.end(), [](const Entity& a, const Entity& b) {
        if (a.start != b.start) {
            return a.start < b.start;
        }
        return (a.end - a.start) < (b.end - b.start);
    });

    return results;
}

bool IsValidHashtagText(const std::u16string& text) {
    std::u16string candidate = text;
    if (candidate.empty() || candidate.front() != u'#') {
        candidate = u"#" + text;
    }
    std::vector<Entity> hashtags = HashtagsInText(candidate, /*checkingUrlOverlap=*/true);
    if (hashtags.size() != 1) {
        return false;
    }
    const Entity& entity = hashtags.front();
    return entity.start == 0 && entity.end == static_cast<int32_t>(candidate.length());
}

namespace {

// Collects the start offset -> match length (in UTF-16 code units) of every
// non-overlapping emoji sequence in `text`, mirroring the `emojiMap` built in
// TwitterTextParser.java's parseTweet.
std::vector<std::pair<int32_t, int32_t>> FindEmojiSpans(const std::u16string& text) {
    std::vector<std::pair<int32_t, int32_t>> spans;

    MatcherPtr matcher = NewMatcherFor(EmojiPattern());
    if (!matcher) {
        return spans;
    }

    UErrorCode status = U_ZERO_ERROR;
    uregex_setText(matcher.get(), AsUChar(const_cast<std::u16string&>(text)), static_cast<int32_t>(text.length()), &status);
    if (U_FAILURE(status)) {
        return spans;
    }

    UBool found = uregex_find(matcher.get(), 0, &status);
    while (found && U_SUCCESS(status)) {
        UErrorCode rangeStatus = U_ZERO_ERROR;
        int32_t start = uregex_start(matcher.get(), 0, &rangeStatus);
        int32_t end = uregex_end(matcher.get(), 0, &rangeStatus);
        if (U_FAILURE(rangeStatus)) {
            break;
        }
        spans.emplace_back(start, end - start);
        found = uregex_findNext(matcher.get(), &status);
    }

    return spans;
}

bool IsInvalidCharacter(UChar32 codePoint) {
    return codePoint == 0xFFFE || codePoint == 0xFEFF || codePoint == 0xFFFF;
}

} // namespace

int64_t TweetLength(const std::u16string& text, int64_t transformedUrlLength) {
    ParseResults results = ParseTweet(text);
    (void)transformedUrlLength; // TwitterText.m's tweetLength: shares the same
                                // transformedURLLength as parseTweet's default
                                // (v3) configuration; a caller-supplied override
                                // is not currently exposed through the JS spec.
    return results.weightedLength;
}

ParseResults ParseTweet(const std::u16string& text) {
    if (text.empty()) {
        return ParseResults{0, 0, true, 0, 0, 0, 0};
    }

    UErrorCode normStatus = U_ZERO_ERROR;
    const UNormalizer2* nfc = unorm2_getNFCInstance(&normStatus);
    std::u16string normalized = text;
    if (U_SUCCESS(normStatus) && nfc != nullptr) {
        UErrorCode preflightStatus = U_ZERO_ERROR;
        int32_t needed = unorm2_normalize(nfc, AsUChar(const_cast<std::u16string&>(text)), static_cast<int32_t>(text.length()), nullptr, 0, &preflightStatus);
        if (preflightStatus == U_BUFFER_OVERFLOW_ERROR || U_SUCCESS(preflightStatus)) {
            std::u16string candidate(static_cast<size_t>(std::max(needed, 0)), u'\0');
            UErrorCode normalizeStatus = U_ZERO_ERROR;
            unorm2_normalize(nfc, AsUChar(const_cast<std::u16string&>(text)), static_cast<int32_t>(text.length()), AsUChar(candidate), needed, &normalizeStatus);
            if (U_SUCCESS(normalizeStatus)) {
                normalized = candidate;
            }
        }
    }

    int32_t tweetLength = static_cast<int32_t>(normalized.length());
    if (tweetLength == 0) {
        return ParseResults{0, 0, true, 0, 0, 0, 0};
    }

    // Config values from the bundled v3.json (see TwitterTextConfig.h).
    const int64_t scale = kConfigScale;
    const int64_t maxWeightedTweetLength = kConfigMaxWeightedTweetLength;
    const int64_t scaledMaxWeightedTweetLength = maxWeightedTweetLength * scale;
    const int64_t transformedUrlWeight = kConfigTransformedURLLength * scale;

    std::vector<Entity> urlEntities = UrlsInText(normalized);
    std::vector<std::pair<int32_t, int32_t>> emojiSpans = FindEmojiSpans(normalized);

    bool hasInvalidCharacters = false;
    int64_t weightedCount = 0;
    int32_t offset = 0;
    int32_t validOffset = 0;

    size_t nextUrlIndex = 0;
    size_t nextEmojiIndex = 0;

    while (offset < tweetLength) {
        int64_t charWeight = kConfigDefaultWeight;

        if (nextUrlIndex < urlEntities.size() && urlEntities[nextUrlIndex].start == offset) {
            int32_t urlLength = urlEntities[nextUrlIndex].end - urlEntities[nextUrlIndex].start;
            weightedCount += transformedUrlWeight;
            offset += urlLength;
            if (weightedCount <= scaledMaxWeightedTweetLength) {
                validOffset += urlLength;
            }
            nextUrlIndex++;
            continue;
        }

        if (offset < tweetLength) {
            const UChar* normalizedChars = AsUChar(normalized);
            UChar32 codePoint;
            int32_t next = offset;
            U16_NEXT(normalizedChars, next, tweetLength, codePoint);
            int32_t offsetDelta = next - offset;

            // Skip past any emoji spans we've already scanned over (e.g.
            // because their start fell inside a URL entity consumed above).
            while (nextEmojiIndex < emojiSpans.size() && emojiSpans[nextEmojiIndex].first < offset) {
                nextEmojiIndex++;
            }

            int32_t emojiLength = -1;
            if (nextEmojiIndex < emojiSpans.size() && emojiSpans[nextEmojiIndex].first == offset) {
                charWeight = kConfigDefaultWeight;
                emojiLength = emojiSpans[nextEmojiIndex].second;
                nextEmojiIndex++;
            } else {
                charWeight = WeightForCodePoint(codePoint);
            }

            weightedCount += charWeight;

            hasInvalidCharacters = hasInvalidCharacters || IsInvalidCharacter(codePoint);

            if (emojiLength != -1) {
                offsetDelta = emojiLength;
            }
            offset += offsetDelta;
            if (!hasInvalidCharacters && weightedCount <= scaledMaxWeightedTweetLength) {
                validOffset += offsetDelta;
            }
        }
    }

    int32_t normalizedTweetOffset = static_cast<int32_t>(text.length()) - static_cast<int32_t>(normalized.length());
    int64_t scaledWeightedLength = weightedCount / scale;
    bool isValid = !hasInvalidCharacters && scaledWeightedLength <= maxWeightedTweetLength;
    int64_t permillage = maxWeightedTweetLength > 0 ? (scaledWeightedLength * 1000 / maxWeightedTweetLength) : 0;

    int32_t displayEnd = offset + normalizedTweetOffset - 1;
    int32_t validEnd = validOffset + normalizedTweetOffset - 1;

    return ParseResults{
        scaledWeightedLength,
        permillage,
        isValid,
        0,
        displayEnd < 0 ? 0 : displayEnd,
        0,
        validEnd < 0 ? 0 : validEnd,
    };
}

} // namespace TwitterText
