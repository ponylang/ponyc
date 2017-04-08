#pragma once

#include "rapidcheck/detail/Results.h"

namespace rc {
namespace state {

template <typename Cmds, typename Model>
void applyAll(const Cmds &commands, Model &state) {
  for (const auto &command : commands) {
    command->checkPreconditions(state);
    command->apply(state);
  }
}

template <typename Cmds>
void runAll(const Cmds &commands,
            const typename Cmds::value_type::element_type::Model &state,
            typename Cmds::value_type::element_type::Sut &sut) {
  runAll(commands, fn::constant(state), sut);
}

template <typename Cmds, typename MakeInitialState, typename>
void runAll(const Cmds &commands,
            const MakeInitialState &makeInitialState,
            typename Cmds::value_type::element_type::Sut &sut) {
  auto currentState = makeInitialState();
  for (const auto &command : commands) {
    command->checkPreconditions(currentState);
    command->run(currentState, sut);
    command->apply(currentState);
  }
}

template <typename Cmds>
bool isValidSequence(const Cmds &commands,
                     const typename Cmds::value_type::element_type::Model &s0) {
  return isValidSequence(commands, fn::constant(s0));
}

template <typename Cmds, typename MakeInitialState, typename>
bool isValidSequence(const Cmds &commands,
                     const MakeInitialState &makeInitialState) {
  auto state = makeInitialState();
  try {
    applyAll(commands, state);
  } catch (const ::rc::detail::CaseResult &result) {
    if (result.type == ::rc::detail::CaseResult::Type::Discard) {
      return false;
    }
    throw;
  }

  return true;
}

template <typename Model, typename Sut>
void showValue(const std::vector<
                   std::shared_ptr<const state::Command<Model, Sut>>> &commands,
               std::ostream &os) {
  for (const auto &command : commands) {
    command->show(os);
    os << std::endl;
  }
}

} // namespace state
} // namespace rc
