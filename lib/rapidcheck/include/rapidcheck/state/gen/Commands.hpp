#pragma once

#include <algorithm>

#include "rapidcheck/Random.h"
#include "rapidcheck/GenerationFailure.h"
#include "rapidcheck/shrinkable/Transform.h"

namespace rc {
namespace state {
namespace gen {
namespace detail {

template <typename MakeInitialState, typename GenFunc>
class CommandsGen {
public:
  using Model = Decay<typename std::result_of<MakeInitialState()>::type>;
  using CommandGen = typename std::result_of<GenFunc(Model)>::type;
  using Cmd = typename CommandGen::ValueType::element_type::CommandType;
  using CmdSP = std::shared_ptr<const Cmd>;
  using Sut = typename Cmd::Sut;

  template <typename InitialStateArg, typename GenFuncArg>
  CommandsGen(InitialStateArg &&initialState, GenFuncArg &&genFunc)
      : m_initialState(std::forward<InitialStateArg>(initialState))
      , m_genFunc(std::forward<GenFuncArg>(genFunc)) {}

  Shrinkable<Commands<Cmd>> operator()(const Random &random, int size) const {
    auto sequenceShrinkable = shrinkable::shrinkRecur(
        CommandSequence(m_initialState, random, m_genFunc, size),
        [](const CommandSequence &commandSequence) {
          return commandSequence.shrinks();
        });

    return shrinkable::map(
        std::move(sequenceShrinkable),
        [](const CommandSequence &sequence) { return sequence.asCommands(); });
  }

private:
  class CommandEntry {
  public:
    CommandEntry(Random random, Shrinkable<CmdSP> &&shrinkable)
        : m_random(std::move(random))
        , m_shrinkable(std::move(shrinkable))
        , m_command(m_shrinkable.value()) {}

    const Random &random() const { return m_random; }
    const Shrinkable<CmdSP> &shrinkable() const { return m_shrinkable; }
    const CmdSP &command() const { return m_command; }

    void safeApply(Model &model) const {
      try {
        m_command->apply(model);
      } catch (const ::rc::detail::CaseResult &result) {
        if (result.type == ::rc::detail::CaseResult::Type::Discard) {
          throw GenerationFailure(
              result.description +
              "\n\nAsserting preconditions in apply(...) is deprecated. "
              "Implement 'void checkPreconditions(const Model &s0) const' and "
              "do it there instead. See the documentation for more details.");
        }
        throw;
      }
    }

    void setShrinkable(Shrinkable<CmdSP> &&s) {
      m_shrinkable = std::move(s);
      m_command = m_shrinkable.value();
    }

  private:
    Random m_random;
    Shrinkable<CmdSP> m_shrinkable;
    CmdSP m_command;
  };

  class CommandSequence {
  public:
    CommandSequence(const MakeInitialState &initState,
                    const Random &random,
                    const GenFunc &func,
                    int sz)
        : m_initialState(initState)
        , m_genFunc(func)
        , m_size(sz) {
      auto r = random;
      std::size_t count = (r.split().next() % (m_size + 1)) + 1;
      generateInitial(r, count);
    }

    Commands<Cmd> asCommands() const {
      Commands<Cmd> cmds;
      cmds.reserve(m_entries.size());
      std::transform(begin(m_entries),
                     end(m_entries),
                     std::back_inserter(cmds),
                     [](const CommandEntry &entry) { return entry.command(); });
      return cmds;
    }

    Seq<CommandSequence> shrinks() const {
      return seq::concat(shrinksRemoving(), shrinksIndividual());
    }

  private:
    // Generates the initial sequence of commands
    void generateInitial(const Random &random, std::size_t count) {
      m_entries.reserve(count);

      auto state = m_initialState();
      auto r = random;
      while (m_entries.size() < count) {
        m_entries.push_back(nextEntry(r.split(), state));
        m_entries.back().safeApply(state);
      }
    }

    // Tries to generate the next entry given the specified random and state.
    CommandEntry nextEntry(const Random &random, Model &state) const {
      auto r = random;
      const auto gen = m_genFunc(state);
      // TODO configurability?
      for (int tries = 0; tries < 100; tries++) {
        if (auto entry = entryForState(r.split(), state)) {
          return *entry;
        }
      }

      // TODO better error message
      throw GenerationFailure("Failed to generate command after 100 tries.");
    }

    // Returns the state at the given index.
    Model stateAt(std::size_t n) const {
      auto state = m_initialState();
      for (std::size_t i = 0; i < n; i++) {
        m_entries[i].safeApply(state);
      }

      return state;
    }

    // Repairs entries so that the command sequence is valid.
    void repairEntries() {
      auto state = m_initialState();
      for (std::size_t i = 0; i < m_entries.size(); i++) {
        if (!repairEntryAt(i, state)) {
          m_entries.erase(begin(m_entries) + i--);
        }
      }
    }

    // Repairs the entry at the given index by regenerating it if necessary.
    bool repairEntryAt(std::size_t i, Model &state) {
      auto &entry = m_entries[i];
      if (!isValidCommand(*entry.command(), state)) {
        return regenerateEntryAt(i, state);
      }
      entry.safeApply(state);
      return true;
    }

    // Regenerates the entry at the given index.
    bool regenerateEntryAt(std::size_t i, Model &state) {
      auto &entry = m_entries[i];
      if (auto newEntry = entryForState(entry.random(), state)) {
        entry = std::move(*newEntry);
        entry.safeApply(state);
        return true;
      }

      return false;
    }

    // Returns the state after applying commands up to the given index.
    Maybe<CommandEntry> entryForState(const Random &random,
                                      const Model &state) const {
      try {
        auto shrinkable = m_genFunc(state)(random, m_size);
        CommandEntry entry(random, std::move(shrinkable));
        if (isValidCommand(*entry.command(), state)) {
          return entry;
        }
      } catch (const GenerationFailure &) {
      } catch (const rc::detail::CaseResult &result) {
        if (result.type != rc::detail::CaseResult::Type::Discard) {
          throw;
        }
      }

      return Nothing;
    }

    // Returns the shrinks possible by removing subranges of commands.
    Seq<CommandSequence> shrinksRemoving() const {
      const auto copy = *this;
      return seq::map(seq::subranges(0, m_entries.size()),
                      [=](const std::pair<std::size_t, std::size_t> &r) {
                        auto shrunk = copy;
                        shrunk.removeRange(r.first, r.second);
                        return shrunk;
                      });
    }

    // Removes the commands in the given range.
    void removeRange(std::size_t l, std::size_t r) {
      m_entries.erase(begin(m_entries) + l, begin(m_entries) + r);
      repairEntries();
    }

    // Returns the shrinks possible by replacing a command with shrunk version.
    Seq<CommandSequence> shrinksIndividual() const {
      const auto copy = *this;
      return seq::mapcat(
          seq::range<std::size_t>(0, m_entries.size()),
          [=](std::size_t i) {
            const auto preState = std::make_shared<Model>(copy.stateAt(i));
            auto validReplacements =
                seq::filter(copy.m_entries[i].shrinkable().shrinks(),
                            [=](const Shrinkable<CmdSP> &s) {
                              return isValidCommand(*s.value(), *preState);
                            });

            return seq::map(std::move(validReplacements),
                            [=](Shrinkable<CmdSP> &&shrink) {
                              auto shrunk = copy;
                              shrunk.replaceShrinkable(i, std::move(shrink));
                              return shrunk;
                            });
          });
    }

    // Replaces the shrinkable at the given index.
    void replaceShrinkable(std::size_t i, Shrinkable<CmdSP> shrinkable) {
      m_entries[i].setShrinkable(std::move(shrinkable));
      repairEntries();
    }

    MakeInitialState m_initialState;
    GenFunc m_genFunc;
    int m_size;
    std::vector<CommandEntry> m_entries;
  };

  MakeInitialState m_initialState;
  GenFunc m_genFunc;
};

} // namespace detail

template <typename Model, typename GenerationFunc>
auto commands(const Model &initialState, GenerationFunc &&genFunc)
    -> detail::CommandsGenFor<decltype(genFunc(initialState))> {
  return commands(fn::constant(initialState),
                  std::forward<GenerationFunc>(genFunc));
}

template <typename MakeInitialState, typename GenerationFunc>
auto commands(MakeInitialState &&initialState, GenerationFunc &&genFunc)
    -> detail::CommandsGenFor<decltype(genFunc(initialState()))> {
  return detail::CommandsGen<Decay<MakeInitialState>, Decay<GenerationFunc>>(
      std::forward<MakeInitialState>(initialState),
      std::forward<GenerationFunc>(genFunc));
}

} // namespace gen
} // namespace state
} // namespace rc
