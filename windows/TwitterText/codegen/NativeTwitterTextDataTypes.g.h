/*
 * This file is auto-generated from a NativeModule spec file in js.
 *
 * This is a C++ Spec class that should be used with MakeTurboModuleProvider to register native modules
 * in a way that also verifies at compile time that the native module matches the interface required
 * by the TurboModule JS spec.
 */
#pragma once
// clang-format off

#include <string>
#include <optional>
#include <functional>
#include <vector>

namespace TwitterTextCodegen {

struct ParseResults {
    double weightedLength;
    double permillage;
    bool isValid;
    double displayTextRangeStart;
    double displayTextRangeEnd;
    double validTextRangeStart;
    double validTextRangeEnd;
};

struct Entity {
    std::string type;
    double start;
    double end;
    std::string value;
    std::optional<std::string> listSlug;
};

} // namespace TwitterTextCodegen
