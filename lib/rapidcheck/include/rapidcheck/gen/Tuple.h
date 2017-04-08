#pragma once

#include "rapidcheck/Gen.h"

namespace rc {
namespace gen {

/// Given a number of generators, returns a generator for a tuple with the value
/// types of those generators.
template <typename... Ts>
Gen<std::tuple<Ts...>> tuple(Gen<Ts>... gens);

/// Given two generators, returns a generator for a pair with the value types of
/// those generators.
template <typename T1, typename T2>
Gen<std::pair<T1, T2>> pair(Gen<T1> gen1, Gen<T2> gen2);

} // namespace gen
} // namespace rc

#include "Tuple.hpp"
