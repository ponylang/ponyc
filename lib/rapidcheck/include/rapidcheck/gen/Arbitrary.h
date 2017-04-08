#pragma once

#include "rapidcheck/Gen.h"

namespace rc {

/// Specialize this template to provide default arbitrary generators for custom
/// types. Specializations should have a static method `Gen<T> arbitrary()` that
/// returns a suitable generator for generating arbitrary values of `T`.
template <typename T>
struct Arbitrary;

namespace gen {

/// Returns a generator for arbitrary values of `T`.
template <typename T>
decltype(Arbitrary<T>::arbitrary()) arbitrary();

} // namespace gen
} // namespace rc

#include "Arbitrary.hpp"
