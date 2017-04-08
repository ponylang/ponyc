#pragma once

#include "rapidcheck/detail/IntSequence.h"

namespace rc {
namespace detail {

template <typename TupleT,
          typename Callable,
          typename Indexes =
              MakeIndexSequence<std::tuple_size<Decay<TupleT>>::value>>
struct ApplyTupleImpl;

template <typename... Ts, std::size_t... Indexes, typename Callable>
struct ApplyTupleImpl<std::tuple<Ts...>, Callable, IndexSequence<Indexes...>> {
  using ReturnType = typename std::result_of<Callable(Ts &&...)>::type;

  static ReturnType apply(std::tuple<Ts...> &&tuple, Callable &&callable) {
    return callable(std::move(std::get<Indexes>(tuple))...);
  }
};

template <typename... Ts, std::size_t... Indexes, typename Callable>
struct ApplyTupleImpl<std::tuple<Ts...> &,
                      Callable,
                      IndexSequence<Indexes...>> {
  using ReturnType = typename std::result_of<Callable(Ts &...)>::type;

  static ReturnType apply(std::tuple<Ts...> &tuple, Callable &&callable) {
    return callable(std::get<Indexes>(tuple)...);
  }
};

template <typename... Ts, std::size_t... Indexes, typename Callable>
struct ApplyTupleImpl<const std::tuple<Ts...> &,
                      Callable,
                      IndexSequence<Indexes...>> {
  using ReturnType = typename std::result_of<Callable(const Ts &...)>::type;

  static ReturnType apply(const std::tuple<Ts...> &tuple, Callable &&callable) {
    return callable(std::get<Indexes>(tuple)...);
  }
};

/// Applies the given tuple as arguments to the given callable.
template <typename TupleT, typename Callable>
typename ApplyTupleImpl<TupleT, Callable>::ReturnType
applyTuple(TupleT &&tuple, Callable &&callable) {
  return ApplyTupleImpl<TupleT, Callable>::apply(
      std::forward<TupleT>(tuple), std::forward<Callable>(callable));
}

} // namespace detail
} // namespace rc
