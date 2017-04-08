#pragma once

#include <string>

namespace rc {

/// Checks the given testable and returns `true` on success and `false` on
/// failure. This method will also print information about the testing to
/// stderr.
template <typename Testable>
bool check(Testable &&testable);

/// Same as `check(Testable &&)` but also takes a description of the property
/// that is being tested as the first parameter. This will be used in the
/// output.
template <typename Testable>
bool check(const std::string &description, Testable &&testable);

} // namespace rc

#include "Check.hpp"
