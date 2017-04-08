#pragma once

#include <cmath>
#include <locale>

#include "rapidcheck/seq/Transform.h"
#include "rapidcheck/seq/Create.h"

namespace rc {
namespace shrink {
namespace detail {

template <typename T>
class TowardsSeq {
public:
  using UInt = typename std::make_unsigned<T>::type;

  TowardsSeq(T value, T target)
      : m_value(value)
      , m_diff((target < value) ? (value - target) : (target - value))
      , m_down(target < value) {}

  Maybe<T> operator()() {
    if (m_diff == 0) {
      return Nothing;
    }

    T ret = m_down ? (m_value - m_diff) : (m_value + m_diff);
    m_diff /= 2;
    return ret;
  }

private:
  T m_value;
  UInt m_diff;
  bool m_down;
};

template <typename Container>
class RemoveChunksSeq {
public:
  template <typename ContainerArg>
  explicit RemoveChunksSeq(ContainerArg &&elements)
      : m_elements(std::forward<Container>(elements))
      , m_start(0)
      , m_size(m_elements.size()) {}

  Maybe<Container> operator()() {
    if (m_size == 0) {
      return Nothing;
    }

    Container elements;
    elements.reserve(m_elements.size() - m_size);
    const auto start = begin(m_elements);
    const auto fin = end(m_elements);
    elements.insert(end(elements), start, start + m_start);
    elements.insert(end(elements), start + m_start + m_size, fin);

    if ((m_size + m_start) >= m_elements.size()) {
      m_size--;
      m_start = 0;
    } else {
      m_start++;
    }
    return elements;
  }

private:
  Container m_elements;
  std::size_t m_start;
  std::size_t m_size;
};

template <typename Container, typename Shrink>
class EachElementSeq {
public:
  using T = typename std::result_of<Shrink(
      typename Container::value_type)>::type::ValueType;

  template <typename ContainerArg, typename ShrinkArg>
  explicit EachElementSeq(ContainerArg &&elements, ShrinkArg &&shrink)
      : m_elements(std::forward<Container>(elements))
      , m_shrink(std::forward<ShrinkArg>(shrink))
      , m_i(0) {}

  Maybe<Container> operator()() {
    auto value = next();
    if (!value) {
      return Nothing;
    }

    auto elements = m_elements;
    elements[m_i - 1] = std::move(*value);
    return elements;
  }

private:
  Maybe<T> next() {
    while (true) {
      auto value = m_shrinks.next();
      if (value) {
        return value;
      }

      if (m_i >= m_elements.size()) {
        return Nothing;
      }

      m_shrinks = m_shrink(m_elements[m_i++]);
    }
  }

  Container m_elements;
  Shrink m_shrink;
  Seq<T> m_shrinks;
  std::size_t m_i;
};

template <typename T>
Seq<T> integral(T value, std::true_type) {
  // The check for > min() is important since -min() == min() and we never
  // want to include self
  if ((value < 0) && (value > std::numeric_limits<T>::min())) {
    // Drop the zero from towards and put that before the negation value
    // so we don't have duplicate zeroes
    return seq::concat(seq::just<T>(static_cast<T>(0), static_cast<T>(-value)),
                       seq::drop(1, shrink::towards<T>(value, 0)));
  }

  return shrink::towards<T>(value, 0);
}

template <typename T>
Seq<T> integral(T value, std::false_type) {
  return shrink::towards<T>(value, 0);
}

} // namespace detail

template <typename Container>
Seq<Container> removeChunks(Container elements) {
  return makeSeq<detail::RemoveChunksSeq<Container>>(std::move(elements));
}

template <typename Container, typename Shrink>
Seq<Container> eachElement(Container elements, Shrink shrink) {
  return makeSeq<detail::EachElementSeq<Container, Shrink>>(std::move(elements),
                                                            std::move(shrink));
}

template <typename T>
Seq<T> towards(T value, T target) {
  return makeSeq<detail::TowardsSeq<T>>(value, target);
}

template <typename T>
Seq<T> integral(T value) {
  return detail::integral(value, std::is_signed<T>());
}

template <typename T, typename>
Seq<T> integral(T value);

template <typename T>
Seq<T> real(T value) {
  std::vector<T> shrinks;

  if (value != 0) {
    shrinks.push_back(0.0);
  }

  if (value < 0) {
    shrinks.push_back(-value);
  }

  T truncated = std::trunc(value);
  if (std::abs(truncated) < std::abs(value)) {
    shrinks.push_back(truncated);
  }

  return seq::fromContainer(shrinks);
}

Seq<bool> boolean(bool value) { return value ? seq::just(false) : Seq<bool>(); }

template <typename T>
Seq<T> character(T value) {
  const auto &locale = std::locale::classic();
  auto shrinks = seq::cast<T>(seq::concat(
      seq::fromContainer(std::string("abc")),
      // TODO this seems a bit hacky
      std::islower(static_cast<char>(value), locale)
          ? Seq<char>()
          : seq::just(static_cast<char>(std::tolower(value, locale))),
      seq::fromContainer(std::string("ABC123 \n"))));

  return seq::takeWhile(std::move(shrinks), [=](T x) { return x != value; });
}

} // namespace shrink
} // namespace rc
