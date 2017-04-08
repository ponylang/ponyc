#pragma once

namespace rc {
namespace detail {

/// TMP utility for passing around a list of types. The type is actually defined
/// so that it can be used in tag dispatching et.c.
template <typename... Types>
struct TypeList {};

namespace tl {

template <typename T>
struct ToTupleImpl;

template <typename... Types>
struct ToTupleImpl<TypeList<Types...>> {
  using Type = std::tuple<Types...>;
};

/// Turns a `TypeList` into am `std::tuple`.
template <typename T>
using ToTuple = typename ToTupleImpl<T>::Type;

template <typename T>
struct DecayAllImpl;

template <typename... Types>
struct DecayAllImpl<TypeList<Types...>> {
  using Type = TypeList<typename std::decay<Types>::type...>;
};

/// Decays all the types in a `TypeList`.
template <typename T>
using DecayAll = typename DecayAllImpl<T>::Type;

} // namespace tl
} // namespace detail
} // namespace rc
