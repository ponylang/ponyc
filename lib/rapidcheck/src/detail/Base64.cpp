#include "Base64.h"

#include <algorithm>

#include "ParseException.h"

namespace rc {
namespace detail {
namespace {

const std::int16_t reverseAlphabet[] = {
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, 62, -1, -1, -1, -1, 52, 53, 54, 55, 56, 57, 58, 59, 60,
    61, -1, -1, -1, -1, -1, -1, -1, 0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10,
    11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1,
    63, -1, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42,
    43, 44, 45, 46, 47, 48, 49, 50, 51, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1};

} // namespace

const char *kBase64Alphabet =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+_";

std::string base64Encode(const std::vector<std::uint8_t> &data) {
  const auto size = data.size();
  const auto outputSize = (size * 4 + 2) / 3;
  std::string output;
  output.reserve(outputSize);

  for (std::size_t i = 0; i < size; i += 3) {
    int nbits = 0;
    std::uint32_t bits = 0;
    for (std::size_t j = i; j < std::min<std::size_t>(size, i + 3); j++) {
      bits |= data[j] << nbits;
      nbits += 8;
    }

    while (nbits > 0) {
      output.append(1, kBase64Alphabet[bits & 0x3F]);
      nbits -= 6;
      bits = bits >> 6;
    }
  }

  return output;
}

std::vector<std::uint8_t> base64Decode(const std::string &data) {
  if ((data.size() % 4) == 1) {
    throw ParseException(data.size(),
                         "Invalid number of characters for Base64");
  }

  const auto size = data.size();
  const auto outputSize = (size * 3) / 4;
  std::vector<std::uint8_t> output;
  output.reserve(outputSize);

  for (std::size_t i = 0; i < size; i += 4) {
    int nbits = 0;
    std::uint32_t bits = 0;
    for (std::size_t j = i; j < std::min<std::size_t>(size, i + 4); j++) {
      const auto x = reverseAlphabet[static_cast<std::uint8_t>(data[j])];
      if (x == -1) {
        throw ParseException(j, "Invalid Base64 character");
      }
      bits |= x << nbits;
      nbits += 6;
    }

    while (nbits >= 8) {
      output.push_back(static_cast<std::uint8_t>(bits));
      nbits -= 8;
      bits = bits >> 8;
    }
  }

  return output;
}

} // namespace detail
} // namespace rc
