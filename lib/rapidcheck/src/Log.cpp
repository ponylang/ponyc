#include "rapidcheck/Log.h"

#include "rapidcheck/detail/ImplicitParam.h"
#include "rapidcheck/detail/PropertyContext.h"

namespace rc {
namespace detail {

std::ostream &log() {
  return ImplicitParam<param::CurrentPropertyContext>::value()->logStream();
}

void log(const std::string &msg) {
  log() << msg << std::endl;
}

} // namespace detail
} // namespace rc
