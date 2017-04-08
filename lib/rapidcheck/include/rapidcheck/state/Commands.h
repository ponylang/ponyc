#pragma once

#include "rapidcheck/state/Command.h"

namespace rc {
namespace state {

/// Alias for a vector of `shared_ptr`s to commands of a particular type. It is
/// a pointer to a const command since commands are treated as values and are
/// thus immutable.
template <typename CommandType>
using Commands = std::vector<std::shared_ptr<const CommandType>>;

/// Applies each command in `commands` to the given state. `commands` is assumed
/// to be a container supporting `begin` and `end` containing pointers to
/// commands appropriate for the given state.
template <typename Cmds, typename Model>
void applyAll(const Cmds &commands, Model &state);

/// Runs each command in `command` on the given system under test assuming the
/// given state. `commands` is assumed to be a container supporting `begin` and
/// `end` containing pointers to commands appropriate for the given state and
/// system under test.
template <typename Cmds>
void runAll(const Cmds &commands,
            const typename Cmds::value_type::element_type::Model &state,
            typename Cmds::value_type::element_type::Sut &sut);

/// Same as `runAll(Commands, Model, Sut)` but instead of taking the intitial
/// state directly takes a callable which returns the initial state. This has
/// the advantage of also working with move-only models.
template <typename Cmds,
          typename MakeInitialState,
          typename = typename std::enable_if<!std::is_same<
              Decay<MakeInitialState>,
              typename Cmds::value_type::element_type::Model>::value>::type>
void runAll(const Cmds &commands,
            const MakeInitialState &makeInitialState,
            typename Cmds::value_type::element_type::Sut &sut);

/// Checks whether the given command sequence is valid for the given state.
template <typename Cmds>
bool isValidSequence(const Cmds &commands,
                     const typename Cmds::value_type::element_type::Model &s0);

/// Same as `isValidSequence(Commands, Model)` but instead of taking the
/// intitial state directly takes a callable which returns the initial state.
/// This has the advantage of also working with move-only models.
template <typename Cmds,
          typename MakeInitialState,
          typename = typename std::enable_if<!std::is_same<
              Decay<MakeInitialState>,
              typename Cmds::value_type::element_type::Model>::value>::type>
bool isValidSequence(const Cmds &commands,
                     const MakeInitialState &makeInitialState);

} // namespace state
} // namespace rc

#include "Commands.hpp"
