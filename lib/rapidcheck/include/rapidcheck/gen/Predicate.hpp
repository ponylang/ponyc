#pragma once

namespace rc {
namespace gen {
namespace detail {

template <typename T, typename Predicate>
class FilterGen {
public:
  template <typename PredicateArg>
  FilterGen(Gen<T> gen, PredicateArg &&predicate)
      : m_predicate(std::forward<PredicateArg>(predicate))
      , m_gen(std::move(gen)) {}

  Shrinkable<T> operator()(const Random &random, int size) const {
    Random r(random);
    int currentSize = size;
    for (int tries = 0; tries < 100; tries++) {
      auto shrinkable =
          shrinkable::filter(m_gen(r.split(), currentSize), m_predicate);

      if (shrinkable) {
        return std::move(*shrinkable);
      }
      currentSize++;
    }

    throw GenerationFailure(
        "Gave up trying to generate value satisfying predicate.");
  }

private:
  Predicate m_predicate;
  Gen<T> m_gen;
};

template <typename Value>
class DistinctPredicate {
public:
  template <typename ValueArg,
            typename = typename std::enable_if<
                !std::is_same<Decay<ValueArg>, DistinctPredicate>::value>::type>
  DistinctPredicate(ValueArg &&value)
      : m_value(std::forward<ValueArg>(value)) {}

  template <typename T>
  bool operator()(const T &other) const {
    return !(other == m_value);
  }

private:
  Value m_value;
};

} // namespace detail

template <typename T, typename Predicate>
Gen<T> suchThat(Gen<T> gen, Predicate &&pred) {
  return detail::FilterGen<T, Decay<Predicate>>(std::move(gen),
                                                std::forward<Predicate>(pred));
}

template <typename T, typename Predicate>
Gen<T> suchThat(Predicate &&pred) {
  return gen::suchThat(gen::arbitrary<T>(), std::forward<Predicate>(pred));
}

template <typename T>
Gen<T> nonEmpty(Gen<T> gen) {
  return gen::suchThat(std::move(gen), [](const T &x) { return !x.empty(); });
}

template <typename T>
Gen<T> nonEmpty() {
  return gen::suchThat(gen::arbitrary<T>(),
                       [](const T &x) { return !x.empty(); });
}

template <typename T, typename Value>
Gen<T> distinctFrom(Gen<T> gen, Value &&value) {
  return gen::suchThat(
      std::move(gen),
      detail::DistinctPredicate<Decay<Value>>(std::forward<Value>(value)));
}

template <typename Value>
Gen<Decay<Value>> distinctFrom(Value &&value) {
  return gen::suchThat(
      gen::arbitrary<Decay<Value>>(),
      detail::DistinctPredicate<Decay<Value>>(std::forward<Value>(value)));
}

template <typename T>
Gen<T> nonZero() {
  return gen::suchThat(gen::arbitrary<T>(), [](T x) { return x != T(0); });
}

template <typename T>
Gen<T> positive() {
  return gen::suchThat(gen::arbitrary<T>(), [](T x) { return x > T(0); });
}

template <typename T>
Gen<T> negative() {
  return gen::suchThat(gen::arbitrary<T>(), [](T x) { return x < T(0); });
}

template <typename T>
Gen<T> nonNegative() {
  return gen::suchThat(gen::arbitrary<T>(), [](T x) { return x >= T(0); });
}

} // namespace gen
} // namespace rc
