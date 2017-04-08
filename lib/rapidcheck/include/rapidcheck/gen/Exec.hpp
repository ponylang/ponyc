#pragma once

#include "rapidcheck/gen/detail/Recipe.h"
#include "rapidcheck/gen/detail/ExecRaw.h"

namespace rc {
namespace gen {

template <typename Callable>
Gen<Decay<rc::detail::ReturnType<Callable>>> exec(Callable &&callable) {
  using namespace detail;
  using T = rc::detail::ReturnType<Callable>;

  return gen::map(execRaw(std::forward<Callable>(callable)),
                  [](std::pair<T, Recipe> &&p) { return std::move(p.first); });
}

} // namespace gen
} // namespace rc
