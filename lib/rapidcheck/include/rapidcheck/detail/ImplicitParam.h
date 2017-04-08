#pragma once

#include <stack>
#include <vector>

#include "Utility.h"

/// @file
/// The classes in this file provide a way to pass around "implicit" parameters
/// to functions. A lot of the magic in RapidCheck relies on being able to pass
/// data around behind the users back, not in any explicit parameter given to
/// properties or generators.
///
/// Semantically, this is essentially dynamic scoping in some LISP dialects or
/// implicit parameters in Haskell.
///
/// An example:
///     {
///         ImplicitParam<Foo> foo("foo");
///         ImplicitParam<Bar> bar("bar");
///         // Until foo and bar goes out of scope, every invocation of
///         // ImplicitParam<...>::value() will see the respective bindings.
///         someFunction(); // This function will also see that
///     }
///     // Here, since foo and bar went out of scope, the previous bindings were
///     // restored.

namespace rc {
namespace detail {

/// Used to create a new "scope" of implicit parameters. Constructing a new
/// `ImplicitScope` object is equivalent to creating a binding of every existing
/// `ImplicitParam` instance. This allows you to run with a fresh set parameters
/// during a particular scope, a reset, so to speak.
///
/// Please note that the default values will be created on demand, that is, they
/// will be created first when `ImplicitParam<T>::value()` is called, not when
/// an instance of `ImplicitScope` is created.
///
/// Constructing an instance of this class in any other way than on the stack is
/// not recommended and should not be done unless you know what you are doing.
class ImplicitScope {
  template <typename Param>
  friend class ImplicitParam;

public:
  ImplicitScope();
  ~ImplicitScope();

private:
  RC_DISABLE_COPY(ImplicitScope)

  using Destructor = void (*)();
  using Destructors = std::vector<Destructor>;
  using ScopeStack = std::stack<Destructors, std::vector<Destructors>>;
  static ScopeStack m_scopes;
};

/// Constructing an instance of `ImplicitParam<Param>` establishes a binding for
/// implicit parameter `Param` for the lifetime of that object. This instance
/// should always be constructed on the stack since stack lifecycle semantics
/// are the whole point of this system.
///
/// To create a new implicit parameter, you need to create a struct which
/// describes the parameter. The struct should have a type alias for the type of
/// the values it contains named `ValueType` and also a static function named
/// `defaultValue` which returns the default binding value of the parameter.
///
/// Example:
///     struct MyStringParam
///     {
///         using ValueType = std::string;
///         static std::string defaultValue() { return "default!"; }
///     };
template <typename Param>
class ImplicitParam {
public:
  /// The type of the values of this `ImplicitParam`.
  using ValueType = typename Param::ValueType;

  /// Establishes a binding for `Param`.
  ///
  /// @param value  The value to bind.
  ImplicitParam(ValueType value);

  /// Returns the value of the current binding.
  static ValueType &value();

  ~ImplicitParam();

private:
  RC_DISABLE_COPY(ImplicitParam)

  void init();

  using Binding = std::pair<ValueType, ImplicitScope::ScopeStack::size_type>;
  using StackT = std::stack<Binding, std::vector<Binding>>;
  static StackT m_stack;
};

} // namespace detail
} // namespace rc

#include "ImplicitParam.hpp"
