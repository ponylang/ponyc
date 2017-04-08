#pragma once

namespace rc {
namespace gen {
namespace detail {

template <typename T>
struct DefaultArbitrary;

} // namespace detail

template <typename T>
decltype(Arbitrary<T>::arbitrary()) arbitrary() {
  static const auto instance = rc::Arbitrary<T>::arbitrary();
  return instance;
}

} // namespace gen

template <typename T>
struct Arbitrary {
  static decltype(gen::detail::DefaultArbitrary<T>::arbitrary()) arbitrary() {
    return gen::detail::DefaultArbitrary<T>::arbitrary();
  }
};

} // namespace rc

#include "Arbitrary.hpp"
