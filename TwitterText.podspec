require "json"

package = JSON.parse(File.read(File.join(__dir__, "package.json")))

Pod::Spec.new do |s|
  s.name         = "TwitterText"
  s.version      = package["version"]
  s.summary      = package["description"]
  s.homepage     = package["homepage"]
  s.license      = package["license"]
  s.authors      = package["author"]

  # The Objective-C implementation only touches Foundation (no UIKit), so the
  # same sources build unmodified for react-native-macos; :osx is hardcoded
  # since min_ios_version_supported (from React Native's pod helpers) has no
  # macOS equivalent.
  s.platforms    = { :ios => min_ios_version_supported, :osx => "10.15" }
  s.source       = { :git => "https://github.com/mika-f/react-native-twitter-text.git", :tag => "#{s.version}" }

  s.source_files = [
    "ios/**/*.{h,m,mm,swift,cpp}",
    "submodules/twitter-text/objc/lib/*.{h,m}",
    "submodules/twitter-text/objc/ThirdParty/IFUnicodeURL/IFUnicodeURL/**/*.{h,m,c}",
  ]
  s.private_header_files = "ios/**/*.h"
  s.resources = ["submodules/twitter-text/config/*.json"]

  # Rename twitter-text library classes to avoid collision with the
  # React Native module name "TwitterText". RCTTurboModuleManager uses
  # NSClassFromString(@"TwitterText") during lookup, which would find
  # the library class instead of our module class.
  s.pod_target_xcconfig = {
    "GCC_PREPROCESSOR_DEFINITIONS" => "$(inherited) " \
      "TwitterText=TTLibTwitterText " \
      "TwitterTextEntity=TTLibTwitterTextEntity " \
      "TwitterTextParser=TTLibTwitterTextParser " \
      "TwitterTextParseResults=TTLibTwitterTextParseResults " \
      "TwitterTextConfiguration=TTLibTwitterTextConfiguration " \
      "TwitterTextWeightedRange=TTLibTwitterTextWeightedRange " \
      "TwitterTextEmoji=TTLibTwitterTextEmoji"
  }

  install_modules_dependencies(s)
end
