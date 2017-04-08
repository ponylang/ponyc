#pragma once

#include "rapidcheck/Seq.h"

namespace rc {
namespace seq {

/// Drops the first `n` elements from the given `Seq`.
template <typename T>
Seq<T> drop(std::size_t n, Seq<T> seq);

/// Takes the first `n` elements from the given `Seq`.
template <typename T>
Seq<T> take(std::size_t n, Seq<T> seq);

/// Drops all elements until the given predicate returns true.
template <typename T, typename Predicate>
Seq<T> dropWhile(Seq<T> seq, Predicate &&pred);

/// Takes elements until there is an element which does not match the predicate.
template <typename T, typename Predicate>
Seq<T> takeWhile(Seq<T> seq, Predicate &&pred);

/// Maps the elements of the given `Seq` using the given callable.
template <typename T, typename Mapper>
Seq<Decay<typename std::result_of<Mapper(T)>::type>> map(Seq<T> seq,
                                                         Mapper &&mapper);

/// Takes elements from the given `Seq`s and passes them as arguments to the
/// given callable and returns a `Seq` of such return values. The length of the
/// returned `Seq` will be the length of the shortest `Seq` passed in.
///
/// Fun fact: Also works with no sequences and in that case returns an infinite
/// sequence of the return values of calling the given callable.
template <typename... Ts, typename Zipper>
Seq<Decay<typename std::result_of<Zipper(Ts...)>::type>>
zipWith(Zipper &&zipper, Seq<Ts>... seqs);

/// Skips elements not matching the given predicate from the given stream.
template <typename T, typename Predicate>
Seq<T> filter(Seq<T> seq, Predicate &&pred);

/// Takes `Seq<Seq<T>>` and joins them together into a `Seq<T>`.
template <typename T>
Seq<T> join(Seq<Seq<T>> seqs);

/// Concatenates the given `Seq`s.
template <typename T, typename... Ts>
Seq<T> concat(Seq<T> seq, Seq<Ts>... seqs);

/// Maps each tuple elements of the given to `Seq`s to further `Seq`s and
/// concatenates them into one `Seq`. Sometimes called a "flat map".
template <typename T, typename Mapper>
Seq<typename std::result_of<Mapper(T)>::type::ValueType>
mapcat(Seq<T> seq, Mapper &&mapper);

/// Like `map` but expects the mapping functor to return a `Maybe`. If `Nothing`
/// is returned, the element is skipped. Otherwise, the `Maybe` is unwrapped and
/// included in the resulting `Seq`.
template <typename T, typename Mapper>
Seq<typename std::result_of<Mapper(T)>::type::ValueType>
mapMaybe(Seq<T> seq, Mapper &&mapper);

/// Creates a `Seq` which infinitely repeats the given `Seq`.
template <typename T>
Seq<T> cycle(Seq<T> seq);

/// Returns a `Seq` where each value of the given `Seq` is cast to the given
/// type using `static_cast`.
template <typename T, typename U>
Seq<T> cast(Seq<U> seq);

} // namespace seq
} // namespace rc

#include "Transform.hpp"
