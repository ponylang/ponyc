#pragma once

#include <type_traits>

#include "rapidcheck/detail/IntSequence.h"

namespace rc {

/// Convenience wrapper over std::decay, shorter to type.
template <typename T>
using Decay = typename std::decay<T>::type;

/// Checks that all the parameters are true.
template <bool... Xs>
struct AllTrue
    : public std::is_same<detail::IntSequence<bool, Xs...>,
                          detail::IntSequence<bool, (Xs || true)...>> {};

/// Checks that all the types in `Ts` conforms to type trait `F`.
template <template <typename> class F, typename... Ts>
struct AllIs : public AllTrue<F<Ts>::value...> {};

} // namespace rc
