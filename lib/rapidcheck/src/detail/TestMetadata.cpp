#include "rapidcheck/detail/TestMetadata.h"

namespace rc {
namespace detail {

std::ostream &operator<<(std::ostream &os, const TestMetadata &info) {
  os << "id='" << info.id << "', description='" << info.description << "'";
  return os;
}

bool operator==(const TestMetadata &lhs, const TestMetadata &rhs) {
  return (lhs.id == rhs.id) && (lhs.description == rhs.description);
}

bool operator!=(const TestMetadata &lhs, const TestMetadata &rhs) {
  return !(lhs == rhs);
}

} // namespace detail
} // namespace rc
