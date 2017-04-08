#pragma once

namespace rc {

template <typename T>
class Gen;

namespace detail {

class Any;

} // namespace detail

namespace gen {
namespace detail {

/// Implementations of this class receive callbacks when `operator*` of `Gen` is
/// invoked.
class GenerationHandler {
public:
  /// Invoked in response to a call to `operator*` in `Gen`.
  ///
  /// @param gen  A type erased version (erased to `Any`) of the generator
  ///             that was dereferenced.
  ///
  /// @return An `Any` that will be extracted and returned from the generator.
  ///         The underlying value must (obviously) have the same type as the
  ///         underlying generator.
  virtual rc::detail::Any onGenerate(const Gen<rc::detail::Any> &gen) = 0;

  virtual ~GenerationHandler() = default;
};

namespace param {

/// Current GenerationHandler implicit parameter.
struct CurrentHandler {
  using ValueType = GenerationHandler *;
  static GenerationHandler *defaultValue();
};

} // namespace param
} // namespace detail
} // namespace gen
} // namespace rc
