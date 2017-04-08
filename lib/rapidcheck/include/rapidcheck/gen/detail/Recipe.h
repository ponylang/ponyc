#pragma once

#include <vector>

#include "rapidcheck/Shrinkable.h"
#include "rapidcheck/detail/Any.h"
#include "rapidcheck/Random.h"

namespace rc {
namespace gen {
namespace detail {

/// Describes the recipe for generating a certain value. This consists of vector
/// of "ingredients" which are the shrinkable values picked to produce the value
/// and descriptions of what those values represent. It also includes the
/// `Random` generator to use and the size.
struct Recipe {
  struct Ingredient {
    Ingredient(std::string &&d, Shrinkable<rc::detail::Any> &&s)
        : description(std::move(d))
        , shrinkable(std::move(s)) {}

    /// A description of the shrinkable value.
    std::string description;

    // The shrinkable value itself.
    Shrinkable<rc::detail::Any> shrinkable;

    rc::detail::Any value() const { return shrinkable.value(); }

    Seq<Shrinkable<rc::detail::Any>> shrinks() const {
      return shrinkable.shrinks();
    }
  };

  using Ingredients = std::vector<Ingredient>;

  Random random;
  int size = 0;
  Ingredients ingredients;
  std::size_t numFixed = 0;
};

/// Returns the non-recursive shrinks for the given recipe.
Seq<Recipe> shrinkRecipe(const Recipe &recipe);

} // namespace detail
} // namespace gen
} // namespace rc
