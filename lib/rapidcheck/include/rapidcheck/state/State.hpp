#pragma once

#include <algorithm>
#include <cassert>

#include "rapidcheck/state/gen/Commands.h"
#include "rapidcheck/gen/Exec.h"

namespace rc {
namespace state {

template <typename Model, typename Sut, typename GenFunc>
void check(const Model &initialState, Sut &sut, GenFunc &&generationFunc) {
  check(fn::constant(initialState), sut, std::forward<GenFunc>(generationFunc));
}

template <typename MakeInitialState, typename Sut, typename GenFunc, typename>
void check(MakeInitialState &&makeInitialState,
           Sut &sut,
           GenFunc &&generationFunc) {
  const auto commands =
      *gen::commands(makeInitialState, std::forward<GenFunc>(generationFunc));
  runAll(commands, makeInitialState, sut);
}

template <typename Model, typename Sut>
bool isValidCommand(const Command<Model, Sut> &command, const Model &s0) {
  try {
    command.checkPreconditions(s0);
  } catch (const ::rc::detail::CaseResult &result) {
    if (result.type == ::rc::detail::CaseResult::Type::Discard) {
      return false;
    }
    throw;
  }

  return true;
}

} // namespace state
} // namespace rc
