#pragma once

#include "rapidcheck/shrinkable/Create.h"
#include "rapidcheck/gen/Tuple.h"
#include "rapidcheck/gen/detail/ExecHandler.h"

namespace rc {
namespace gen {
namespace detail {

template <typename Callable>
rc::detail::ReturnType<Callable> execWithArguments(const Callable &callable,
                                                   rc::detail::TypeList<>) {
  return callable();
}

template <typename Callable, typename... Args>
rc::detail::ReturnType<Callable>
execWithArguments(const Callable &callable, rc::detail::TypeList<Args...>) {
  return rc::detail::applyTuple(*gen::arbitrary<std::tuple<Decay<Args>...>>(),
                                callable);
}

template <typename Callable>
std::pair<rc::detail::ReturnType<Callable>, Recipe>
execWithRecipe(Callable callable, Recipe recipe) {
  using namespace rc::detail;
  Recipe resultRecipe(recipe);
  ExecHandler handler(resultRecipe);
  ImplicitParam<param::CurrentHandler> letHandler(&handler);

  return std::make_pair(execWithArguments(callable, ArgTypes<Callable>()),
                        std::move(resultRecipe));
}

template <typename Callable>
Seq<Shrinkable<std::pair<rc::detail::ReturnType<Callable>, Recipe>>>
shrinksOfRecipe(Callable callable, Recipe recipe) {
  return seq::map(shrinkRecipe(std::move(recipe)),
                  [=](Recipe &&shrunkRecipe) {
                    return shrinkableWithRecipe(callable, shrunkRecipe);
                  });
}

template <typename Callable>
Shrinkable<std::pair<rc::detail::ReturnType<Callable>, Recipe>>
shrinkableWithRecipe(Callable callable, Recipe recipe) {
  using T = rc::detail::ReturnType<Callable>;
  return shrinkable::shrink([=] { return execWithRecipe(callable, recipe); },
                            [=](std::pair<T, Recipe> &&p) {
                              return shrinksOfRecipe(callable,
                                                     std::move(p.second));
                            });
}

template <typename Callable>
Gen<std::pair<rc::detail::ReturnType<Callable>, Recipe>>
execRaw(Callable callable) {
  return [=](const Random &random, int size) {
    Recipe recipe;
    recipe.random = random;
    recipe.size = size;
    return shrinkableWithRecipe(callable, recipe);
  };
}

} // namespace detail
} // namespace gen
} // namespace rc
