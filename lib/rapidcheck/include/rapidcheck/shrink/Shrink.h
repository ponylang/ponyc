#pragma once

#include "rapidcheck/Seq.h"

namespace rc {
namespace shrink {

/// Tries to shrink the given container by removing successively smaller
/// consecutive chunks of it.
///
/// `Container` must support the following:
/// - `RandomAccessIterator begin(Container)`
/// - `RandomAccessIterator end(Container)`
/// - `Container::insert(It, It, It)`
/// - `Container::reserve(std::size_t)`
template <typename Container>
Seq<Container> removeChunks(Container elements);

/// Tries to shrink each element of the given container using the given
/// callable to create sequences of shrinks for that element.
///
/// `Container` must support `begin(Container)` and `end(Container)` which must
/// return random access iterators.
///
/// @param elements  The collection whose elements to shrink.
/// @param shrink    A callable which returns a `Seq<T>` given an element
///                  to shrink.
template <typename Container, typename Shrink>
Seq<Container> eachElement(Container elements, Shrink shrink);

/// Shrinks an integral value towards another integral value.
///
/// @param value   The value to shrink.
/// @param target  The integer to shrink towards.
template <typename T>
Seq<T> towards(T value, T target);

/// Shrinks an arbitrary integral value.
template <typename T>
Seq<T> integral(T value);

/// Shrinks an arbitrary real value.
template <typename T>
Seq<T> real(T value);

/// Shrinks an arbitrary boolean value.
inline Seq<bool> boolean(bool value);

/// Shrinks a text character.
template <typename T>
Seq<T> character(T value);

} // namespace shrink
} // namespace rc

#include "Shrink.hpp"
