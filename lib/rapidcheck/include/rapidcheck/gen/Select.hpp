#pragma once

#include "rapidcheck/detail/FrequencyMap.h"
#include "rapidcheck/gen/detail/ScaleInteger.h"

namespace rc {
namespace gen {
namespace detail {

template <typename T>
struct ToIndexable {
  using Output = std::vector<typename T::value_type>;

  template <typename Arg>
  static Output convert(Arg &&container) {
    return Output(begin(std::forward<Arg>(container)),
                  end(std::forward<Arg>(container)));
  }
};

template <typename T, typename Allocator>
struct ToIndexable<std::vector<T, Allocator>> {
  using Output = std::vector<T, Allocator>;

  template <typename Arg>
  static Arg &&convert(Arg &&container) {
    return std::forward<Arg>(container);
  }
};

template <typename T, typename Traits, typename Allocator>
struct ToIndexable<std::basic_string<T, Traits, Allocator>> {
  using Output = std::basic_string<T, Traits, Allocator>;

  template <typename Arg>
  static Arg &&convert(Arg &&container) {
    return std::forward<Arg>(container);
  }
};

template <typename Container>
class ElementOfGen {
public:
  using T = typename Container::value_type;

  template <typename Arg>
  ElementOfGen(Arg &&arg)
      : m_container(std::forward<Arg>(arg)) {}

  Shrinkable<T> operator()(const Random &random, int size) const {
    const auto start = begin(m_container);
    const auto containerSize = end(m_container) - start;
    if (containerSize == 0) {
      throw GenerationFailure("Cannot pick element from empty container.");
    }
    const auto i =
        static_cast<std::size_t>(Random(random).next() % containerSize);
    return shrinkable::just(*(start + i));
  }

private:
  Container m_container;
};

template <typename T>
class WeightedElementGen {
public:
  WeightedElementGen(std::vector<std::size_t> &&frequencies,
                     std::vector<T> &&elements)
      : m_map(std::move(frequencies))
      , m_elements(std::move(elements)) {}

  Shrinkable<T> operator()(const Random &random, int size) const {
    if (m_map.sum() == 0) {
      throw GenerationFailure("Sum of weights is 0");
    }
    return shrinkable::just(static_cast<T>(
        m_elements[m_map.lookup(Random(random).next() % m_map.sum())]));
  }

private:
  rc::detail::FrequencyMap m_map;
  std::vector<T> m_elements;
};

template <typename Container>
class SizedElementOfGen {
public:
  using T = typename Container::value_type;

  template <typename Arg>
  explicit SizedElementOfGen(Arg &&arg)
      : m_container(std::forward<Arg>(arg)) {}

  Shrinkable<T> operator()(const Random &random, int size) const {
    if (m_container.size() == 0) {
      throw GenerationFailure("Cannot pick element from empty container.");
    }

    const auto max = detail::scaleInteger(m_container.size() - 1, size) + 1;
    const auto container = m_container;
    return shrinkable::map(
        shrinkable::shrinkRecur(
            static_cast<std::size_t>(Random(random).next() % max),
            [](std::size_t x) { return shrink::towards<std::size_t>(x, 0); }),
        [=](std::size_t x) { return container[x]; });
  }

private:
  Container m_container;
};

template <typename T>
class OneOfGen {
public:
  template <typename... Ts>
  OneOfGen(Gen<Ts>... gens)
      : m_gens{std::move(gens)...} {}

  Shrinkable<T> operator()(const Random &random, int size) const {
    Random r(random);
    const auto i = static_cast<std::size_t>(r.split().next() % m_gens.size());
    return m_gens[i](r, size);
  }

private:
  std::vector<Gen<T>> m_gens;
};

} // namespace detail

template <typename Container>
Gen<typename Decay<Container>::value_type> elementOf(Container &&container) {
  using Convert = detail::ToIndexable<Decay<Container>>;
  using Impl = detail::ElementOfGen<typename Convert::Output>;
  return Impl(Convert::convert(std::forward<Container>(container)));
}

template <typename T, typename... Ts>
Gen<Decay<T>> element(T &&element, Ts &&... elements) {
  using Vector = std::vector<Decay<T>>;
  return detail::ElementOfGen<Vector>(
      Vector{std::forward<T>(element), std::forward<Ts>(elements)...});
}

template <typename T>
Gen<T>
weightedElement(std::initializer_list<std::pair<std::size_t, T>> pairs) {
  std::vector<std::size_t> frequencies;
  frequencies.reserve(pairs.size());
  std::vector<T> elements;
  elements.reserve(pairs.size());

  for (auto &pair : pairs) {
    frequencies.push_back(pair.first);
    elements.push_back(std::move(pair.second));
  }

  return detail::WeightedElementGen<T>(std::move(frequencies),
                                       std::move(elements));
}

template <typename Container>
Gen<typename Decay<Container>::value_type>
sizedElementOf(Container &&container) {
  using Convert = detail::ToIndexable<Decay<Container>>;
  using Impl = detail::SizedElementOfGen<typename Convert::Output>;
  return Impl(Convert::convert(std::forward<Container>(container)));
}

template <typename T, typename... Ts>
Gen<Decay<T>> sizedElement(T &&element, Ts &&... elements) {
  using Vector = std::vector<Decay<T>>;
  return gen::sizedElementOf(
      Vector{std::forward<T>(element), std::forward<Ts>(elements)...});
}

template <typename T, typename... Ts>
Gen<T> oneOf(Gen<T> gen, Gen<Ts>... gens) {
  return detail::OneOfGen<T>(std::move(gen), std::move(gens)...);
}

template <typename T>
Gen<T>
weightedOneOf(std::initializer_list<std::pair<std::size_t, Gen<T>>> pairs) {
  return gen::join(gen::weightedElement(pairs));
}

template <typename T, typename... Ts>
Gen<T> sizedOneOf(Gen<T> gen, Gen<Ts>... gens) {
  return gen::join(gen::sizedElement(std::move(gen), std::move(gens)...));
}

} // namespace gen
} // namespace rc
