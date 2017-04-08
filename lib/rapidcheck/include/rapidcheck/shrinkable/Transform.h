#pragma once

#include "rapidcheck/Shrinkable.h"

namespace rc {
namespace shrinkable {

/// Maps the given shrinkable recursively using the given mapping callable.
template <typename T, typename Mapper>
Shrinkable<Decay<typename std::result_of<Mapper(T)>::type>>
map(Shrinkable<T> shrinkable, Mapper &&mapper);

/// Returns a shrinkable equal to the given shrinkable but with the shrinks
/// (lazily) mapped by the given mapping callable. Since the value is not mapped
/// also the output type is the same as the output type.
template <typename T, typename Mapper>
Shrinkable<T> mapShrinks(Shrinkable<T> shrinkable, Mapper &&mapper);

/// Recursively filters the given shrinkable using the given predicate. Any
/// subtree with a root for which the predicate returns false is discarded,
/// including the passed in root which is why this function returns a `Maybe`.
template <typename T, typename Predicate>
Maybe<Shrinkable<T>> filter(Shrinkable<T> shrinkable, Predicate &&pred);

/// Monadic bind. Takes a `Shrinkable<T>` and a function from a `T` to a
/// `Shrinkable<U>` and returns a `Shrinkable<U>` that shrinks by first
/// shrinking the first value and then shrinking the second value.
template <typename T, typename Mapper>
Shrinkable<typename std::result_of<Mapper(T)>::type::ValueType>
mapcat(Shrinkable<T> shrinkable, Mapper &&mapper);

/// Given two `Shrinkables`, returns a `Shrinkable` pair that first shrinks the
/// first element and then the second.
template <typename T1, typename T2>
Shrinkable<std::pair<T1, T2>> pair(Shrinkable<T1> s1, Shrinkable<T2> s2);

/// Recursively appends shrinks using the given shrinking callable. The callable
/// should take the value to shrink as the only argument and return a `Seq` of
/// shrinks of that value.
template <typename T, typename Shrink>
Shrinkable<T> postShrink(Shrinkable<T> shrinkable, Shrink &&shrink);

} // namespace shrinkable
} // namespace rc

#include "Transform.hpp"
