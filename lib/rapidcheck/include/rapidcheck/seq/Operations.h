#pragma once

#include "rapidcheck/Seq.h"

namespace rc {
namespace seq {

/// Returns the length of the given sequence. This is an O(n) operation.
template <typename T>
std::size_t length(Seq<T> seq);

/// Calls the given callable once for each element of the sequence.
template <typename T, typename Callable>
void forEach(Seq<T> seq, Callable callable);

/// Returns the last value of the given `Seq` or an uninitialized `Maybe` if the
/// given `Seq` is empty. This is an O(n) operation.
template <typename T>
Maybe<T> last(Seq<T> seq);

/// Returns true if the given `Seq` contains the given element.
template <typename T>
bool contains(Seq<T> seq, const T &value);

/// Returns true if the given predicate matches all elements of the given finite
/// `Seq`.
template <typename T, typename Predicate>
bool all(Seq<T> seq, Predicate pred);

/// Returns true if the given predicate matches any of the elements in the given
/// `Seq`.
template <typename T, typename Predicate>
bool any(Seq<T> seq, Predicate pred);

/// Returns the item at the given index or `Nothing` if the `Seq` ends before
/// that index. O(n) complexity.
template <typename T>
Maybe<T> at(Seq<T> seq, std::size_t index);

} // namespace seq
} // namespace rc

#include "Operations.hpp"
