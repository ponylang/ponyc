#pragma once

#include "rapidcheck/detail/BitStream.h"
#include "rapidcheck/shrinkable/Create.h"
#include "rapidcheck/shrink/Shrink.h"
#include "rapidcheck/gen/Transform.h"
#include "rapidcheck/gen/detail/ScaleInteger.h"

namespace rc {
namespace gen {
namespace detail {

template <typename T>
Shrinkable<T> integral(const Random &random, int size) {
  return shrinkable::shrinkRecur(
      rc::detail::bitStreamOf(random).nextWithSize<T>(size),
      &shrink::integral<T>);
}

extern template Shrinkable<char> integral<char>(const Random &random, int size);
extern template Shrinkable<unsigned char>
integral<unsigned char>(const Random &random, int size);
extern template Shrinkable<short> integral<short>(const Random &random,
                                                  int size);
extern template Shrinkable<unsigned short>
integral<unsigned short>(const Random &random, int size);
extern template Shrinkable<int> integral<int>(const Random &random, int size);
extern template Shrinkable<unsigned int>
integral<unsigned int>(const Random &random, int size);
extern template Shrinkable<long> integral<long>(const Random &random, int size);
extern template Shrinkable<unsigned long>
integral<unsigned long>(const Random &random, int size);
extern template Shrinkable<long long> integral<long long>(const Random &random,
                                                          int size);
extern template Shrinkable<unsigned long long>
integral<unsigned long long>(const Random &random, int size);

template <typename T>
Shrinkable<T> real(const Random &random, int size) {
  // TODO this implementation sucks
  auto stream = rc::detail::bitStreamOf(random);
  const double scale =
      std::min(size, kNominalSize) / static_cast<double>(kNominalSize);
  const double a = static_cast<double>(stream.nextWithSize<int64_t>(size));
  const double b =
      (stream.next<uint64_t>() * scale) / std::numeric_limits<uint64_t>::max();
  const T value = static_cast<T>(a + b);
  return shrinkable::shrinkRecur(value, &shrink::real<T>);
}

extern template Shrinkable<float> real<float>(const Random &random, int size);
extern template Shrinkable<double> real<double>(const Random &random, int size);

Shrinkable<bool> boolean(const Random &random, int size);

template <typename T>
struct DefaultArbitrary {
  // If you ended up here, it means that RapidCheck wanted to generate an
  // arbitrary value of some type but you haven't declared a specialization of
  // Arbitrary for your type. Check the template stack trace to see which type
  // it is.
  static_assert(std::is_integral<T>::value,
                "No Arbitrary specialization for type T");

  static Gen<T> arbitrary() { return integral<T>; }
};

template <>
struct DefaultArbitrary<float> {
  static Gen<float> arbitrary() { return real<float>; }
};

template <>
struct DefaultArbitrary<double> {
  static Gen<double> arbitrary() { return real<double>; }
};

template <>
struct DefaultArbitrary<long double> {
  static Gen<long double> arbitrary() { return real<long double>; }
};

template <>
struct DefaultArbitrary<bool> {
  static Gen<bool> arbitrary() { return boolean; }
};

} // namespace detail

template <typename T>
Gen<T> inRange(T min, T max) {
  return [=](const Random &random, int size) {
    if (max <= min) {
      std::string msg;
      msg += "Invalid range [" + std::to_string(min);
      msg += ", " + std::to_string(max) + ")";
      throw GenerationFailure(msg);
    }

    const auto rangeSize =
        detail::scaleInteger(static_cast<Random::Number>(max) -
                                 static_cast<Random::Number>(min) - 1,
                             size) +
        1;
    const auto value =
        static_cast<T>((Random(random).next() % rangeSize) + min);
    assert(value >= min && value < max);
    return shrinkable::shrinkRecur(
        value, [=](T x) { return shrink::towards<T>(x, min); });
  };
}

} // namespace gen
} // namespace rc
