#pragma once

namespace rc {
namespace detail {

/// We don't want to require C++14 so we make our own version of this.
template <typename T, T... Ints>
struct IntSequence;

/// If T == std::size_t
template <std::size_t... Ints>
using IndexSequence = IntSequence<std::size_t, Ints...>;

template <typename Sequence, std::size_t N, typename Enable = void>
struct MakeIntSequenceImpl;

template <typename IntSequenceT>
struct MakeIntSequenceImpl<IntSequenceT, 0, void> {
  using Sequence = IntSequenceT;
};

template <typename IntT, IntT... Ints, std::size_t N>
struct MakeIntSequenceImpl<IntSequence<IntT, Ints...>,
                           N,
                           typename std::enable_if<N != 0>::type> {

  using Sequence =
      typename MakeIntSequenceImpl<IntSequence<IntT, N - 1, Ints...>,
                                   N - 1>::Sequence;
};

/// Creates an `IntSequence` from 0 to N exclusive.
template <typename T, T N>
using MakeIntSequence =
    typename MakeIntSequenceImpl<IntSequence<T>, N>::Sequence;

/// Alias for MakeIntSequence<std::size_t, N>
template <std::size_t N>
using MakeIndexSequence = MakeIntSequence<std::size_t, N>;

} // namespace detail
} // namespace rc
