#pragma once

#include "rapidcheck/state/Commands.h"

namespace rc {
namespace state {
namespace gen {
namespace detail {

// MSVC HACK - it is prettier but we shouldn't really have to use this type
// alias here but MSVC doesn't seem to understand otherwise
template <typename T>
using CommandsGenFor =
    Gen<Commands<typename T::ValueType::element_type::CommandType>>;

} // namespace detail

/// Generates a valid commands sequence for the given state initial state
/// consisting of commands of the given type.
template <typename Model, typename GenerationFunc>
auto commands(const Model &initialState, GenerationFunc &&genFunc)
    -> detail::CommandsGenFor<decltype(genFunc(initialState))>;

/// Generates a valid commands sequence for the state returned by the given
/// callable consisting of commands of the given type.
template <typename MakeInitialState, typename GenerationFunc>
auto commands(MakeInitialState &&initialState, GenerationFunc &&genFunc)
    -> detail::CommandsGenFor<decltype(genFunc(initialState()))>;

} // namespace gen
} // namespace state
} // namespace rc

#include "Commands.hpp"
