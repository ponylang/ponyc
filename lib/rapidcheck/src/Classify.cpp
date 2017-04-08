#include "rapidcheck/Classify.h"

#include "rapidcheck/detail/ImplicitParam.h"
#include "rapidcheck/detail/PropertyContext.h"

namespace rc {
namespace detail {


void tag(std::initializer_list<Stringified> tags) {
  const auto handler = ImplicitParam<param::CurrentPropertyContext>::value();
  for (const auto &tag : tags) {
    handler->addTag(tag.str);
  }
}

void classify(std::string condition, std::initializer_list<Stringified> tags) {
  const auto handler = ImplicitParam<param::CurrentPropertyContext>::value();
  if (tags.size() == 0) {
    if (!condition.empty()) {
      handler->addTag(std::move(condition));
    }
  } else {
    for (const auto &tag : tags) {
      handler->addTag(tag.str);
    }
  }
}

} // namespace detail
} // namespace rc
