#pragma once

namespace rc {
namespace gen {

/// Returns a generator which randomly selects an element from the given
/// container.
template <typename Container>
Gen<typename Decay<Container>::value_type> elementOf(Container &&container);

/// Returns a generator which randomly selects one of the given elements.
template <typename T, typename... Ts>
Gen<Decay<T>> element(T &&element, Ts &&... elements);

/// Takes a list of elements paired with respective weights and returns a
/// generator which generates one of the elements according to the weights.
template <typename T>
Gen<T>
weightedElement(std::initializer_list<std::pair<std::size_t, T>> pairs);

/// Chooses an element from an initial segment of the given container. The
/// segment starts with length 1 and increases with size. This means that you
/// should place elements that are considered smaller before elements that are
/// considered larger.
template <typename Container>
Gen<typename Decay<Container>::value_type>
sizedElementOf(Container &&container);

/// Chooses an element from an initial segment of the given elements. The
/// segment starts with length 1 and increases with size. This means that you
/// should place elements that are considered smaller before elements that are
/// considered larger.
template <typename T, typename... Ts>
Gen<Decay<T>> sizedElement(T &&element, Ts &&... elements);

/// Returns a generator which randomly generates using one of the specified
/// generators.
template <typename T, typename... Ts>
Gen<T> oneOf(Gen<T> gen, Gen<Ts>... gens);

/// Takes a list of generators paired with respective weights and returns a
/// generator which randomly uses one of the generators according to the weights
/// to generate the final value.
template <typename T>
Gen<T>
weightedOneOf(std::initializer_list<std::pair<std::size_t, Gen<T>>> pairs);

/// Chooses a generator from an initial segment of the given elements. The
/// segment starts with length 1 and increases with size. This means that you
/// should place generators that are considered smaller before generators that
/// are considered larger. This generator is then used to generate the final
/// value.
template <typename T, typename... Ts>
Gen<T> sizedOneOf(Gen<T> gen, Gen<Ts>... gens);

} // namespace gen
} // namespace rc

#include "Select.hpp"
