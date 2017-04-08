#pragma once

#include "rapidcheck/gen/detail/GenerationHandler.h"
#include "rapidcheck/gen/detail/Recipe.h"

namespace rc {
namespace gen {
namespace detail {

/// `GenerationHandler` used to implement `execRaw`.
class ExecHandler : public GenerationHandler {
public:
  ExecHandler(Recipe &recipe);
  rc::detail::Any onGenerate(const Gen<rc::detail::Any> &gen);

private:
  Recipe &m_recipe;
  Random m_random;
  using Iterator = Recipe::Ingredients::iterator;
  Iterator m_it;
};

} // namespace detail
} // namespace gen
} // namespace rc
