#pragma once

#include "rapidcheck/Shrinkable.h"

namespace rc {
namespace shrinkable {

/// Creates a `Shrinkable` from a callable which returns the value and a
/// callable which returns the shrinks.
template <typename Value, typename Shrink>
Shrinkable<typename std::result_of<Value()>::type> lambda(Value &&value,
                                                          Shrink &&shrinks);

/// Creates a `Shrinkable` with no shrinks from a callable which returns the
/// value.
template <typename Value>
Shrinkable<typename std::result_of<Value()>::type> lambda(Value &&value);

/// Creates a `Shrinkable` from an immediate value and an immediate sequence of
/// shrinks.
template <typename T,
          typename Value,
          typename = typename std::enable_if<
              std::is_same<Decay<Value>, T>::value>::type>
Shrinkable<T> just(Value &&value, Seq<Shrinkable<T>> shrinks);

/// Creates a `Shrinkable` from an immediate value with no shrinks.
template <typename T>
Shrinkable<Decay<T>> just(T &&value);

/// Creates a `Shrinkable` from a callable which returns the value and a
/// callable that returns a `Seq<Shrinkable<T>>` when called with the value.
template <typename Value, typename Shrink>
Shrinkable<typename std::result_of<Value()>::type> shrink(Value &&value,
                                                          Shrink &&shrinkf);

/// Creates a `Shrinkable` from an immediate value and a shrinking callable that
/// returns a `Seq<T>` of possible shrinks when called with the value. It does
/// by recursively applying this function to the sequence of shrinks.
template <typename T, typename Shrink>
Shrinkable<Decay<T>> shrinkRecur(T &&value, const Shrink &shrinkf);

} // namespace shrinkable
} // namespace rc

#include "Create.hpp"
