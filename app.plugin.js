/**
 * Expo Config Plugin for @natsuneko-laboratory/react-native-twitter-text.
 *
 * Native module registration, podspec linking, and android source sets
 * are handled automatically by Expo's autolinking.
 * The jsr305 dependency required by twitter-text Java is declared in
 * the library's own build.gradle.
 *
 * This plugin is a pass-through to allow declaring the library
 * as a plugin in app.json without errors.
 */
module.exports = (config) => {
  return config;
};
