#include "rapidcheck/GenerationFailure.h"

#include <string>

namespace rc {

GenerationFailure::GenerationFailure(std::string msg)
    : std::runtime_error(std::move(msg)) {}

} // namespace rc
