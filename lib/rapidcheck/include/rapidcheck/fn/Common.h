#pragma once

namespace rc {
namespace fn {

template <typename T>
class Constant;

/// Returns a functor which always returns the given value, regardless of the
/// arguments passed to it. The returned functor is polymorphic in both arity
/// and types.
template <typename T>
Constant<Decay<T>> constant(T &&value);

} // namespace fn
} // namespace rc

#include "Common.hpp"
