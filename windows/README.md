# react-native-windows support

Unlike iOS (Objective-C) and Android (Java), upstream [twitter-text](https://github.com/twitter/twitter-text)
ships no C++/C# implementation, so this platform's Turbo Module (`TwitterText/`)
carries its own C++ port of the parsing algorithm (`TwitterTextCore.h/.cpp`)
instead of wrapping a bundled native library.

## Design notes

- **Regex patterns** (`TwitterTextCore.cpp`, top section) are transcribed
  character-for-character from
  [`submodules/twitter-text/objc/lib/TwitterText.m`](../submodules/twitter-text/objc/lib/TwitterText.m)
  and [`TwitterTextEmoji.h`](../submodules/twitter-text/objc/lib/TwitterTextEmoji.h).
  Every embedded Unicode literal was normalized to an explicit `\uXXXX`/`\UXXXXXXXX`
  ICU regex escape. Do not hand-edit these without diffing against the
  Objective-C source.
- **Weighted length / `parseTweet`** follows the (simpler, and easier to port
  faithfully) structure of
  [`TwitterTextParser.java`](../submodules/twitter-text/java/src/main/java/com/twitter/twittertext/TwitterTextParser.java)
  rather than the Objective-C composed-character-sequence version.
- **Character weighting** uses the same default (v3) configuration as iOS/Android
  (`submodules/twitter-text/config/v3.json`), hardcoded in `TwitterTextConfig.h`.
- **Unicode/regex engine**: Windows only exposes [ICU's C API](https://learn.microsoft.com/en-us/windows/win32/intl/international-components-for-unicode--icu-)
  (`<icu.h>` / `icu.lib`, built into Windows 10 1903+) — it does not expose ICU's
  C++ classes (`icu::UnicodeString`, `icu::RegexMatcher`, ...), so all ICU calls
  go through the plain C API (`uregex_*`, `unorm2_*`, `uidna_*`) operating on
  `std::u16string` buffers. This is a real OS component, not a bundled/vendored
  dependency — no extra NuGet/vcpkg package is required.
- **Punycode/host-length validation** uses ICU's UTS46 IDNA implementation
  (`uidna_openUTS46`/`uidna_nameToASCII`) as the Windows equivalent of the
  bundled IFUnicodeURL library (iOS) / `java.net.IDN` (Android).

## Regenerating the codegen headers

`windows/TwitterText/codegen/*.g.h` are committed (as is conventional for
react-native-windows native modules) so the project builds without running
codegen first. If `src/NativeTwitterText.ts` changes, regenerate them with:

```bash
yarn react-native codegen-windows
```

and update `TwitterTextModule.h`/`.cpp` to match any signature changes.

## Known limitations

This is a from-scratch C++ port rather than a wrapper around Twitter's own
native library (as iOS/Android are), so:

- It has not been validated against twitter-text's official conformance test
  suite (`submodules/twitter-text/conformance/`) the way the JS implementation
  is.
- `isValidHostAndLength`'s fallback behavior for hosts with disallowed/non-LDH
  characters is a reasonable approximation of IFUnicodeURL/`java.net.IDN`, not
  a byte-for-byte match.

Please file an issue (with a minimal repro) if you find a case where the
Windows implementation disagrees with iOS/Android.
