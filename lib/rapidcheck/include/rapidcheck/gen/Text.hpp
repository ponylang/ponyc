#pragma once

#include <cctype>

#include "rapidcheck/detail/BitStream.h"
#include "rapidcheck/gen/Container.h"

namespace rc {
namespace gen {
namespace detail {

template <typename String>
class StringGen;

template <typename T, typename... Args>
class StringGen<std::basic_string<T, Args...>> {
public:
  using String = std::basic_string<T, Args...>;

  Shrinkable<String> operator()(const Random &random, int size) const {
    auto stream = rc::detail::bitStreamOf(random);
    String str;
    auto length = stream.next<std::size_t>() % (size + 1);
    str.reserve(length);

    for (std::size_t i = 0; i < length; i++) {
      bool small = stream.next<bool>();
      T value;
      do {
        value = small ? stream.next<T>(7) : stream.next<T>();
      } while (value == '\0');
      str.push_back(value);
    }

    return shrinkable::shrinkRecur(
        std::move(str),
        [](const String &s) {
          return seq::concat(shrink::removeChunks(s),
                             shrink::eachElement(s, &shrink::character<T>));
        });
  }
};

template <typename T, typename... Args>
struct DefaultArbitrary<std::basic_string<T, Args...>> {
  static Gen<std::basic_string<T, Args...>> arbitrary() {
    return gen::string<std::basic_string<T, Args...>>();
  }
};

} // namespace detail

template <typename T>
Gen<T> character() {
  return [](const Random &random, int size) {
    auto stream = ::rc::detail::bitStreamOf(random);
    bool small = stream.next<bool>();
    T value;
    while ((value = small ? stream.next<T>(7) : stream.next<T>()) == '\0') {
      // Do nothing
    }

    return shrinkable::shrinkRecur(value, &shrink::character<T>);
  };
}

template <typename String>
Gen<String> string() {
  return detail::StringGen<String>();
}

} // namespace gen
} // namespace rc

extern template rc::Gen<std::string> rc::gen::string<std::string>();
extern template rc::Gen<std::wstring> rc::gen::string<std::wstring>();
extern template struct rc::Arbitrary<std::string>;
extern template struct rc::Arbitrary<std::wstring>;
