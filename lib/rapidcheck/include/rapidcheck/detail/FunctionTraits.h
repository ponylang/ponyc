#pragma once

#include "TypeList.h"

namespace rc {
namespace detail {

template <typename T>
struct MemberFunctionTraitsImpl;

template <typename T, typename Ret, typename... Args>
struct MemberFunctionTraitsImpl<Ret (T::*)(Args...)> {
  using ReturnType = Ret;
  using ArgTypes = TypeList<Args...>;
  using FunctionType = Ret(Args...);
  using Target = T;
};

template <typename T, typename Ret, typename... Args>
struct MemberFunctionTraitsImpl<Ret (T::*)(Args...) const> {
  using ReturnType = Ret;
  using ArgTypes = TypeList<Args...>;
  using FunctionType = Ret(Args...);
  using Target = T;
};

template <typename T>
struct FunctionTraitsImpl
    : public MemberFunctionTraitsImpl<decltype(&T::operator())> {};

// NOTE: Base template is for functors.

template <typename Ret, typename... Args>
struct FunctionTraitsImpl<Ret (*)(Args...)> {
  using ReturnType = Ret;
  using ArgTypes = TypeList<Args...>;
  using FunctionType = Ret(Args...);
};

/// Traits class for capturing various information about a callable object.
template <typename T>
using FunctionTraits = FunctionTraitsImpl<Decay<T>>;

/// The return type of a function.
template <typename T>
using ReturnType = typename FunctionTraits<T>::ReturnType;

/// `TypeList` of argument types for `T`.
template <typename T>
using ArgTypes = typename FunctionTraits<T>::ArgTypes;

/// The signature of T.
template <typename T>
using FunctionType = typename FunctionTraits<T>::FunctionType;

} // namespace detail
} // namespace rc
