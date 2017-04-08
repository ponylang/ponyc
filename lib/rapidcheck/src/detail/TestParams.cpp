#include "rapidcheck/detail/TestParams.h"

#include "rapidcheck/detail/ImplicitParam.h"
#include "rapidcheck/detail/Configuration.h"

namespace rc {
namespace detail {

bool operator==(const TestParams &p1, const TestParams &p2) {
  return (p1.seed == p2.seed) && (p1.maxSuccess == p2.maxSuccess) &&
      (p1.maxSize == p2.maxSize) &&
      (p1.maxDiscardRatio == p2.maxDiscardRatio) &&
      (p1.disableShrinking == p2.disableShrinking);
}

bool operator!=(const TestParams &p1, const TestParams &p2) {
  return !(p1 == p2);
}

std::ostream &operator<<(std::ostream &os, const TestParams &params) {
  os << "seed=" << params.seed << ", maxSuccess=" << params.maxSuccess
     << ", maxSize=" << params.maxSize
     << ", maxDiscardRatio=" << params.maxDiscardRatio
     << ", disableShrinking=" << params.disableShrinking;
  return os;
}

} // namespace detail
} // namespace rc
