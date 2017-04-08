#pragma once

#include <stdexcept>

namespace rc {

/// Thrown to indicate that an appropriate value couldn't be generated.
class GenerationFailure : public std::runtime_error {
public:
  explicit GenerationFailure(std::string msg);
};

} // namespace rc
