#pragma once

#include <string>
#include <iostream>

#include "rapidcheck/detail/Results.h"

namespace rc {
namespace detail {

/// A `PropertyContext` is the hidden interface with which different actions in
/// properties communicate with the framework.
class PropertyContext {
public:
  /// Reports a result.
  ///
  /// @param result  The result.
  ///
  /// @return `true` if the result was handled and reported, `false` it it was
  ///         ignored
  virtual bool reportResult(const CaseResult &result) = 0;

  /// Returns a stream to which additional information can be logged.
  virtual std::ostream &logStream() = 0;

  /// Adds a tag to the current scope.
  virtual void addTag(std::string str) = 0;

  virtual ~PropertyContext() = default;
};

namespace param {

struct CurrentPropertyContext {
  using ValueType = PropertyContext *;
  static PropertyContext *defaultValue();
};

} // namespace param
} // namespace detail
} // namespace rc
