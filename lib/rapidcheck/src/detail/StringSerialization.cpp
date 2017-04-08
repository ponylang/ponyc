#include "StringSerialization.h"

#include "Base64.h"
#include "ParseException.h"

namespace rc {
namespace detail {

std::string reproduceMapToString(
    const std::unordered_map<std::string, Reproduce> &reproduceMap) {
  std::vector<std::uint8_t> data;
  serialize(reproduceMap, std::back_inserter(data));
  return base64Encode(data);
}

std::unordered_map<std::string, Reproduce>
stringToReproduceMap(const std::string &str) {
  const auto data = base64Decode(str);
  std::unordered_map<std::string, Reproduce> reproduceMap;
  try {
    deserialize(begin(data), end(data), reproduceMap);
  } catch (const SerializationException &) {
    throw ParseException(0, "Invalid format");
  }

  return reproduceMap;
}

} // namespace detail
} // namespace rc
