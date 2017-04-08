#pragma once

#include "rapidcheck/Gen.h"

namespace rc {
namespace gen {

/// Generates an integer in a given range.
///
/// @param min  The minimum value, inclusive.
/// @param max  The maximum value, exclusive.
template <typename T>
Gen<T> inRange(T min, T max);

} // namespace gen
} // namespace rc

#include "Numeric.hpp"
