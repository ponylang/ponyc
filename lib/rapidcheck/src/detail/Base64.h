#pragma once

#include <string>
#include <vector>

namespace rc {
namespace detail {

/// The alphabet that is used for Base64.
extern const char *kBase64Alphabet;

/// Encodes the given data as (some variant of) Base64.
std::string base64Encode(const std::vector<std::uint8_t> &data);

/// Decodes the given data as (some variant of) Base64.
std::vector<std::uint8_t> base64Decode(const std::string &data);

} // namespace detail
} // namespace rc
