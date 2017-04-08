#pragma once

#include <type_traits>
#include <iostream>

namespace rc {
namespace detail {

#define RC_SFINAE_TRAIT(Name, expression)                                      \
  struct Name##Impl {                                                          \
    template <typename T, typename = expression>                               \
    static std::true_type test(const T &);                                     \
    static std::false_type test(...);                                          \
  };                                                                           \
                                                                               \
  template <typename T>                                                        \
  using Name = decltype(Name##Impl::test(std::declval<T>()));

RC_SFINAE_TRAIT(IsStreamInsertible, decltype(std::cout << std::declval<T>()))

} // namespace detail
} // namespace rc
