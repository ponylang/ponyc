#include "rapidcheck/gen/detail/ExecHandler.h"

#include "rapidcheck/Gen.h"

namespace rc {
namespace gen {
namespace detail {

ExecHandler::ExecHandler(Recipe &recipe)
    : m_recipe(recipe)
    , m_random(m_recipe.random)
    , m_it(begin(m_recipe.ingredients)) {}

rc::detail::Any ExecHandler::onGenerate(const Gen<rc::detail::Any> &gen) {
  rc::detail::ImplicitScope newScope;

  Random random = m_random.split();
  if (m_it == end(m_recipe.ingredients)) {
    m_it = m_recipe.ingredients.emplace(
        m_it, gen.name(), gen(random, m_recipe.size));
  }
  auto current = m_it++;
  return current->shrinkable.value();
}

} // namespace detail
} // namespace gen
} // namespace rc
