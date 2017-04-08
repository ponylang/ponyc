#pragma once

namespace rc {
namespace gen {

/// Generates a `Maybe` of the type of the given generator. At small sizes, the
/// frequency of `Nothing` is greater than at larger sizes.
template<typename T>
Gen<Maybe<T>> maybe(Gen<T> gen);

} // namespace gen
} // namespace rc

#include "Maybe.hpp"
