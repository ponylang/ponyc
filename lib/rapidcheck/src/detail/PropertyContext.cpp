#include "rapidcheck/detail/PropertyContext.h"

namespace rc {
namespace detail {
namespace param {
namespace {

class DummyPropertyContext : public PropertyContext {
public:
  bool reportResult(const CaseResult &/*result*/) override { return false; }
  std::ostream &logStream() override { return std::cerr; }
  void addTag(std::string /*str*/) override {}
};

} // namespace

PropertyContext *CurrentPropertyContext::defaultValue() {
  static DummyPropertyContext dummyContext;
  return &dummyContext;
}

} // namespace param
} // namespace detail
} // namespace rc
