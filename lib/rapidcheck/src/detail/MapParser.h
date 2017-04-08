#pragma once

#include <string>
#include <map>

namespace rc {
namespace detail {

/// Parses a configuration string into an `std::map<std::string, std::string>`.
///
/// @throws ParseException  On parse failure.
std::map<std::string, std::string> parseMap(const std::string &str);

/// Converts a map to a string.
///
/// @param map          The map to convert.
/// @param doubleQuote  Whether to use double quotes or single quotes.
std::string mapToString(const std::map<std::string, std::string> &map,
                        bool doubleQuote = false);

} // namespace detail
} // namespace rc
