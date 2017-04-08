#pragma once

#include <string>

#include "rapidcheck/detail/Results.h"

namespace rc {
namespace detail {

/// Converts a Reproduce map to a string.
///
/// @throws ParseException on failure to parse.
std::string reproduceMapToString(
    const std::unordered_map<std::string, Reproduce> &reproduceMap);

/// Converts a string Reproduce map to a string.
///
/// @throws ParseException on failure to parse.
std::unordered_map<std::string, Reproduce>
stringToReproduceMap(const std::string &str);

} // namespace detail
} // namespace rc
