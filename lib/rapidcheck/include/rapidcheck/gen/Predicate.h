#pragma once

namespace rc {
namespace gen {

/// Returns a generator that uses the given generator to generate only values
/// that match the given predicate. Throws a `GenerationFailure` if such a value
/// cannot be generated after an unspecified number of tries.
template <typename T, typename Predicate>
Gen<T> suchThat(Gen<T> gen, Predicate &&pred);

/// Convenience function which calls `suchThat(Gen<T>, Predicate)` with
/// `gen::arbitrary<T>`
template <typename T, typename Predicate>
Gen<T> suchThat(Predicate &&pred);

/// Generates a value `x` using the given generator for `x.empty()` is false.
/// Useful with strings, STL containers and other types.
template <typename T>
Gen<T> nonEmpty(Gen<T> gen);

/// Same as `Gen<T> nonEmpty(Gen<T>)` but uses `gen::arbitrary` for the
/// underlying generator.
template <typename T>
Gen<T> nonEmpty();

/// Generates a value using the given generator that is not equal to the given
/// value.
template <typename T, typename Value>
Gen<T> distinctFrom(Gen<T> gen, Value &&value);

/// Generates an arbitrary value using the given generator that is not equal to
/// the given value.
template <typename Value>
Gen<Decay<Value>> distinctFrom(Value &&value);

/// Generates a value that is not equal to `0`.
template <typename T>
Gen<T> nonZero();

/// Generates a value which is greater than `0`.
template <typename T>
Gen<T> positive();

/// Generates a value which is less than `0`.
template <typename T>
Gen<T> negative();

/// Generates a value which is not less than `0`.
template <typename T>
Gen<T> nonNegative();

} // namespace gen
} // namespace rc

#include "Predicate.hpp"
