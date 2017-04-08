#pragma once

#include "rapidcheck/Gen.h"

namespace rc {
namespace gen {

/// Returns a generator which always returns the given value with no shrinks.
template <typename T>
Gen<Decay<T>> just(T &&value);

/// Creates from a callable that returns another generator. The callable is only
/// called lazily on actual generation. This is useful when implementing
/// recursive generators where a generator must reference itself.
template <typename Callable>
Gen<typename std::result_of<Callable()>::type::ValueType>
lazy(Callable &&callable);

} // namespace gen
} // namespace rc

#include "Create.hpp"
