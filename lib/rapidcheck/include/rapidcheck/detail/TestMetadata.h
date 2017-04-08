#pragma once

#include <string>
#include <iostream>

namespace rc {
namespace detail {

/// Describes a property.
struct TestMetadata {
  /// A unique identifier for the test.
  std::string id;
  /// A description of the test.
  std::string description;
};

std::ostream &operator<<(std::ostream &os, const TestMetadata &info);
bool operator==(const TestMetadata &lhs, const TestMetadata &rhs);
bool operator!=(const TestMetadata &lhs, const TestMetadata &rhs);

} // namespace detail
} // namespace rc
