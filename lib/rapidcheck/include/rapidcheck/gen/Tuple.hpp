#pragma once

#include "rapidcheck/shrinkable/Create.h"
#include "rapidcheck/gen/Arbitrary.h"
#include "rapidcheck/gen/Transform.h"
#include "rapidcheck/Random.h"

namespace rc {
namespace gen {
namespace detail {

template <typename Indexes, typename... Ts>
class TupleShrinkable;

template <typename Indexes, typename... Ts>
class TupleGen;

template <std::size_t I, typename Indexes, typename... Ts>
class TupleShrinkSeq;

template <std::size_t I, typename... Ts, std::size_t... Indexes>
class TupleShrinkSeq<I, rc::detail::IndexSequence<Indexes...>, Ts...> {
public:
  using Tuple = std::tuple<Ts...>;
  using T = typename std::tuple_element<I, Tuple>::type;

  template <typename... Args>
  explicit TupleShrinkSeq(Args &&... args)
      : m_original(std::forward<Args>(args)...)
      , m_shrinks(std::get<I>(m_original).shrinks()) {}

  Maybe<Shrinkable<Tuple>> operator()() {
    auto value = m_shrinks.next();
    if (!value) {
      m_shrinks = Seq<Shrinkable<T>>();
      return Nothing;
    }

    auto shrink = m_original;
    std::get<I>(shrink) = std::move(*value);
    using IndexSeq = rc::detail::MakeIndexSequence<sizeof...(Ts)>;
    using ShrinkableImpl = TupleShrinkable<IndexSeq, Ts...>;
    auto shrinkable =
        makeShrinkable<ShrinkableImpl>(std::move(std::get<Indexes>(shrink))...);
    return shrinkable;
  }

private:
  std::tuple<Shrinkable<Ts>...> m_original;
  Seq<Shrinkable<T>> m_shrinks;
};

template <typename... Ts, std::size_t... Indexes>
class TupleShrinkable<rc::detail::IndexSequence<Indexes...>, Ts...> {
public:
  template <typename... Args>
  explicit TupleShrinkable(Args &&... args)
      : m_shrinkables(std::forward<Args>(args)...) {}

  std::tuple<Ts...> value() const {
    return std::make_tuple(std::get<Indexes>(m_shrinkables).value()...);
  }

  Seq<Shrinkable<std::tuple<Ts...>>> shrinks() const {
    using IndexSeq = rc::detail::IndexSequence<Indexes...>;
    return seq::concat(makeSeq<TupleShrinkSeq<Indexes, IndexSeq, Ts...>>(
        std::get<Indexes>(m_shrinkables)...)...);
  }

private:
  template <std::size_t N>
  static Seq<std::tuple<Shrinkable<Ts>...>>
  shrinkComponent(const std::tuple<Shrinkable<Ts>...> &tuple) {
    using T = typename std::tuple_element<N, std::tuple<Ts...>>::type;
    return seq::map(std::get<N>(tuple).shrinks(),
                    [=](Shrinkable<T> &&cshrink) {
                      auto shrink(tuple);
                      std::get<N>(shrink) = cshrink;
                      return shrink;
                    });
  }

  std::tuple<Shrinkable<Ts>...> m_shrinkables;
};

template <typename... Ts, std::size_t... Indexes>
class TupleGen<rc::detail::IndexSequence<Indexes...>, Ts...> {
public:
  template <typename... Args>
  explicit TupleGen(Args &&... args)
      : m_gens(std::forward<Args>(args)...) {}

  Shrinkable<std::tuple<Ts...>> operator()(const Random &random,
                                           int size) const {
    auto r = random;
    Random randoms[sizeof...(Ts)];
    for (std::size_t i = 0; i < sizeof...(Ts); i++) {
      randoms[i] = r.split();
    }

    using ShrinkableImpl =
        TupleShrinkable<rc::detail::IndexSequence<Indexes...>, Ts...>;
    return makeShrinkable<ShrinkableImpl>(
        std::get<Indexes>(m_gens)(randoms[Indexes], size)...);
  }

private:
  std::tuple<Gen<Ts>...> m_gens;
};

// Specialization for empty tuples.
template <>
class TupleGen<rc::detail::IndexSequence<>> {
public:
  Shrinkable<std::tuple<>> operator()(const Random &/*random*/, int /*size*/) const {
    return shrinkable::just(std::tuple<>());
  }
};

template <typename... Ts>
struct DefaultArbitrary<std::tuple<Ts...>> {
  static Gen<std::tuple<Decay<Ts>...>> arbitrary() {
    return gen::tuple(gen::arbitrary<Decay<Ts>>()...);
  }
};

template <typename T1, typename T2>
struct DefaultArbitrary<std::pair<T1, T2>> {
  static Gen<std::pair<Decay<T1>, Decay<T2>>> arbitrary() {
    return gen::pair(gen::arbitrary<Decay<T1>>(), gen::arbitrary<Decay<T2>>());
  }
};

} // namespace detail

template <typename... Ts>
Gen<std::tuple<Ts...>> tuple(Gen<Ts>... gens) {
  return detail::TupleGen<rc::detail::MakeIndexSequence<sizeof...(Ts)>, Ts...>(
      std::move(gens)...);
}

template <typename T1, typename T2>
Gen<std::pair<T1, T2>> pair(Gen<T1> gen1, Gen<T2> gen2) {
  return gen::map(gen::tuple(std::move(gen1), std::move(gen2)),
                  [](std::tuple<T1, T2> &&t) {
                    return std::make_pair(std::move(std::get<0>(t)),
                                          std::move(std::get<1>(t)));
                  });
}

} // namespace gen
} // namespace rc
