#pragma once

namespace rc {
namespace state {
namespace gen {
namespace detail {

template <typename... Ts>
using TypeList = ::rc::detail::TypeList<Ts...>;

template <typename Cmd, typename Args>
struct MakeCommand;

template <typename Cmd, typename... Args>
struct MakeCommand<Cmd, TypeList<Args...>> {
  static std::shared_ptr<const typename Cmd::CommandType>
  make(const Args &... args) {
    return make(std::is_constructible<Cmd, Args...>(), args...);
  }

  static std::shared_ptr<const typename Cmd::CommandType>
  make(std::true_type, const Args &... args) {
    return std::make_shared<Cmd>(args...);
  }

  static std::shared_ptr<const typename Cmd::CommandType>
  make(std::false_type, const Args &... args) {
    return std::make_shared<Cmd>();
  }
};

template <typename Args, typename... Cmds>
class OneOfCmdGen;

// GCC HACK - Variadics + lambdas don't work very well so this has to be a
// separate class that stores args as a tuple and uses applyTuple and all that.
// When we drop support for old crashing compilers (i.e. GCC) we can just inline
// this as a lambda instead. Much shorter.
template <typename... Args, typename Cmd, typename... Cmds>
class OneOfCmdGen<TypeList<Args...>, Cmd, Cmds...> {
private:
  using CmdSP = std::shared_ptr<const typename Cmd::CommandType>;

public:
  template <typename... Ts>
  OneOfCmdGen(Ts &&... args)
      : m_args(std::forward<Ts>(args)...) {}

  Shrinkable<CmdSP> operator()(const Random &random, int size) const {
    using MakeFunc = CmdSP (*)(const Args &...);
    using ArgsList = TypeList<Args...>;
    static const MakeFunc makeFuncs[] = {&MakeCommand<Cmd, ArgsList>::make,
                                         &MakeCommand<Cmds, ArgsList>::make...};
    auto r = random;
    std::size_t n = r.split().next() % (sizeof...(Cmds) + 1);
    const auto args = m_args;
    return rc::gen::exec([=] {
      return rc::detail::applyTuple(args, makeFuncs[n]);
    })(r, size); // TODO monadic bind
  }

private:
  std::tuple<Args...> m_args;
};

template <typename Cmd, typename... Cmds>
class ExecOneOf {
private:
  using CmdSP = std::shared_ptr<const typename Cmd::CommandType>;

public:
  template <typename... Args>
  Gen<CmdSP> operator()(Args &&... args) const {
    using Generator = OneOfCmdGen<TypeList<Decay<Args>...>, Cmd, Cmds...>;
    return Generator(std::forward<Args>(args)...);
  }
};

} // namespace detail

template <typename Cmd, typename... Cmds>
Gen<std::shared_ptr<const typename Cmd::CommandType>>
execOneOf(const typename Cmd::Model &state) {
  static detail::ExecOneOf<Cmd, Cmds...> execOneOfObject;
  return execOneOfObject(state);
}

template <typename Cmd, typename... Cmds>
detail::ExecOneOf<Cmd, Cmds...> execOneOfWithArgs() {
  return detail::ExecOneOf<Cmd, Cmds...>();
}

} // namespace gen
} // namespace state
} // namespace rc
