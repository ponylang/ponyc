#pragma once

#include "rapidcheck/fn/Common.h"
#include "rapidcheck/shrinkable/Create.h"

namespace rc {
namespace gen {

template <typename T>
Gen<Decay<T>> just(T &&value) {
  return fn::constant(shrinkable::just(std::forward<T>(value)));
}

template <typename Callable>
Gen<typename std::result_of<Callable()>::type::ValueType>
lazy(Callable &&callable) {
  return
      [=](const Random &random, int size) { return callable()(random, size); };
}

} // namespace gen
} // namespace rc
