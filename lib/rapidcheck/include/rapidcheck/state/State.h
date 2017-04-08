#pragma once

#include <memory>

#include "rapidcheck/Gen.h"

namespace rc {
namespace state {

/// Tests a stateful system. This function has assertion semantics (i.e. a
/// failure is equivalent to calling `RC_FAIL` and success is equivalent to
/// `RC_SUCCEED`) so it is intended to be used from a property.
///
/// @param initialState    The initial model state.
/// @param sut             The system under test.
/// @param generationFunc  A callable which takes the current model state as a
///                        parameter and returns a generator for a (possibly)
///                        suitable command.
template <typename Model, typename Sut, typename GenFunc>
void check(const Model &initialState, Sut &sut, GenFunc &&generationFunc);

/// Equivalent to `check(Model, Sut, GenFunc)` but instead of taking the state
/// directly, takes a callable returning the state.
template <typename MakeInitialState,
          typename Sut,
          typename GenFunc,
          typename = decltype(
              std::declval<GenFunc>()(std::declval<MakeInitialState>()()))>
void check(MakeInitialState &&makeInitialState, Sut &sut, GenFunc &&generationFunc);

/// Checks whether command is valid for the given state.
template <typename Model, typename Sut>
bool isValidCommand(const Command<Model, Sut> &command, const Model &s0);

} // namespace state
} // namespace rc

#include "State.hpp"
