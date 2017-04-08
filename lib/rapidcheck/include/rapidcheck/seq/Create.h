#pragma once

#include "rapidcheck/Seq.h"

namespace rc {
namespace seq {

/// Returns a `Seq` which repeatedly yields the given value in an infinite
/// sequence.
template <typename T>
Seq<Decay<T>> repeat(T &&value);

/// Returns a `Seq` which returns just the given values.
template <typename T, typename... Ts>
Seq<Decay<T>> just(T &&value, Ts &&... values);

/// Creates a sequence from the given STL-like container.
template <typename Container>
Seq<Decay<typename Decay<Container>::value_type>>
fromContainer(Container &&container);

/// Creates a sequence from the given iterator range.
template <typename Iterator>
Seq<typename std::iterator_traits<Iterator>::value_type>
fromIteratorRange(Iterator start, Iterator end);

/// Returns an infinite sequence by repeatedly applying the given callable to
/// the given value, i.e. [x, f(x), f(f(x))...].
template <typename T, typename Callable>
Seq<Decay<T>> iterate(T &&x, Callable &&f);

/// Returns a `Seq` of the range of values `[start, end)`. `T` must support
/// prefix increment, prefix decrement, `operator<` and `operator==`. `start`
/// may be greater than `end`.
template <typename T>
Seq<T> range(T start, T end);

// TODO move to .cpp files below

/// Returns a sequence of indexes starting from `0` and increasing.
inline Seq<std::size_t> index();

/// Returns a sequence of all the possible subranges (pair of indexes) of the
/// given range from larger to smaller. The largest subrange (the first one)
/// will be the entire range and the last group of subranges will have length
/// `1`. If `start == end`, the returned `Seq` will be empty.
inline Seq<std::pair<std::size_t, std::size_t>> subranges(std::size_t start,
                                                          std::size_t end);

} // namespace seq
} // namespace rc

#include "Create.hpp"
