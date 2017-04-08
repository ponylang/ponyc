#pragma once

#include "rapidcheck/gen/Tuple.h"
#include "rapidcheck/detail/ApplyTuple.h"

namespace rc {
namespace gen {
namespace detail {

// Member variables
template <typename Type, typename T>
class Lens<T(Type::*)> {
public:
  typedef T(Type::*MemberPtr);
  typedef Type Target;
  typedef T ValueType;

  Lens(MemberPtr ptr)
      : m_ptr(ptr) {}

  template <typename Arg>
  void set(Target &obj, Arg &&arg) const {
    obj.*m_ptr = std::move(std::forward<Arg>(arg));
  }

private:
  MemberPtr m_ptr;
};

// Member functions with single argument
template <typename Type, typename Ret, typename T>
class Lens<Ret (Type::*)(T)> {
public:
  typedef Ret (Type::*MemberPtr)(T);
  typedef Type Target;
  typedef Decay<T> ValueType;

  Lens(MemberPtr ptr)
      : m_ptr(ptr) {}

  template <typename Arg>
  void set(Target &obj, Arg &&arg) const {
    (obj.*m_ptr)(std::forward<Arg>(arg));
  }

private:
  MemberPtr m_ptr;
};

// Member functions with multiple arguments
template <typename Type, typename Ret, typename T1, typename T2, typename... Ts>
class Lens<Ret (Type::*)(T1, T2, Ts...)> {
public:
  typedef Ret (Type::*MemberPtr)(T1, T2, Ts...);
  typedef Type Target;
  typedef std::tuple<Decay<T1>, Decay<T2>, Decay<Ts>...> ValueType;

  Lens(MemberPtr ptr)
      : m_ptr(ptr) {}

  template <typename Arg>
  void set(Target &obj, Arg &&arg) const {
    rc::detail::applyTuple(
        std::forward<Arg>(arg),
        [&](Decay<T1> &&arg1, Decay<T2> &&arg2, Decay<Ts> &&... args) {
          (obj.*m_ptr)(std::move(arg1), std::move(arg2), std::move(args)...);
        });
  }

private:
  MemberPtr m_ptr;
};

template <typename Member, typename SourceType>
struct Binding {
  Binding(Lens<Member> &&l, Gen<SourceType> &&g)
      : lens(std::move(l))
      , gen(std::move(g)) {}

  Lens<Member> lens;
  Gen<SourceType> gen;
};

template <typename T>
T &deref(T &x) {
  return x;
}

template <typename T>
T &deref(std::unique_ptr<T> &x) {
  return *x;
}

template <typename T>
T &deref(std::shared_ptr<T> &x) {
  return *x;
}

template <typename T, typename Indexes, typename... Lenses>
class BuildMapper;

template <typename T, typename... Lenses, std::size_t... Indexes>
class BuildMapper<T, rc::detail::IndexSequence<Indexes...>, Lenses...> {
public:
  BuildMapper(const Lenses &... lenses)
      : m_lenses(lenses...) {}

  T operator()(std::tuple<T, typename Lenses::ValueType...> &&tuple) const {
    T &obj = std::get<0>(tuple);
    auto dummy = {(std::get<Indexes>(m_lenses).set(
                       deref(obj), std::move(std::get<Indexes + 1>(tuple))),
                   0)...};
    (void)dummy; // Silence warning

    return std::move(obj);
  }

private:
  std::tuple<Lenses...> m_lenses;
};

} // namespace detail

template <typename T, typename... Args>
Gen<T> construct(Gen<Args>... gens) {
  return gen::map(gen::tuple(std::move(gens)...),
                  [](std::tuple<Args...> &&argsTuple) {
                    return rc::detail::applyTuple(
                        std::move(argsTuple),
                        [](Args &&... args) { return T(std::move(args)...); });
                  });
}

template <typename T, typename Arg, typename... Args>
Gen<T> construct() {
  return gen::construct<T>(gen::arbitrary<Arg>(), gen::arbitrary<Args>()...);
}

template <typename T, typename... Args>
Gen<std::unique_ptr<T>> makeUnique(Gen<Args>... gens) {
  return gen::map(gen::tuple(std::move(gens)...),
                  [](std::tuple<Args...> &&argsTuple) {
                    return rc::detail::applyTuple(
                        std::move(argsTuple),
                        [](Args &&... args) {
                          return std::unique_ptr<T>(new T(std::move(args)...));
                        });
                  });
}

template <typename T, typename... Args>
Gen<std::shared_ptr<T>> makeShared(Gen<Args>... gens) {
  return gen::map(gen::tuple(std::move(gens)...),
                  [](std::tuple<Args...> &&argsTuple) {
                    return rc::detail::applyTuple(std::move(argsTuple),
                                                  [](Args &&... args) {
                                                    return std::make_shared<T>(
                                                        std::move(args)...);
                                                  });
                  });
}

template <typename Member, typename T>
detail::Binding<Member, T> set(Member member, Gen<T> gen) {
  return detail::Binding<Member, T>(detail::Lens<Member>(member),
                                    std::move(gen));
}

template <typename Member>
detail::Binding<Member, typename detail::Lens<Member>::ValueType>
set(Member member) {
  using T = typename detail::Lens<Member>::ValueType;
  return set(member, gen::arbitrary<T>());
}

template <typename T, typename... Members, typename... SourceTypes>
Gen<T> build(Gen<T> gen, const detail::Binding<Members, SourceTypes> &... bs) {
  using Mapper =
      detail::BuildMapper<T,
                          rc::detail::MakeIndexSequence<sizeof...(Members)>,
                          detail::Lens<Members>...>;

  // GCC HACK - Variadics + lambdas don't work very well in GCC so we use an old
  // fashioned function object (the Mapper thing) to prevent GCC from crashing.
  // Use lambdas and applyTuple once we drop support for the crashing ones.
  return gen::map(gen::tuple(std::move(gen), std::move(bs.gen)...),
                  Mapper(bs.lens...));
}

template <typename T, typename... Members, typename... SourceTypes>
Gen<T> build(const detail::Binding<Members, SourceTypes> &... bs) {
  return build<T>(fn::constant(shrinkable::lambda([] { return T(); })), bs...);
}

} // namespace gen
} // namespace rc
