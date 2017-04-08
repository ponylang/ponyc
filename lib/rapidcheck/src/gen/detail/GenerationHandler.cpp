#include "rapidcheck/gen/detail/GenerationHandler.h"

#include <stdexcept>

#include "rapidcheck/detail/Any.h"

namespace rc {
namespace gen {
namespace detail {

using Any = rc::detail::Any;

/// Default handler. Just throws exception.
class NullGenerationHandler : public GenerationHandler {
  Any onGenerate(const Gen<rc::detail::Any> &/*gen*/) override {
    throw std::runtime_error("operator* is not allowed in this context");
  }
};

namespace param {

GenerationHandler *CurrentHandler::defaultValue() {
  static NullGenerationHandler nullHandler;
  return &nullHandler;
}

} // namespace param
} // namespace detail
} // namespace gen
} // namespace rc
