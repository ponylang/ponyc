#pragma once

namespace rc {

/// Tag struct that can be used to construct different types to an uninitialized
/// or empty state.
struct NothingType {
  /// Explicit conversion to false.
  explicit operator bool() const { return false; }
};

/// Singleton NothingType value.
constexpr NothingType Nothing = NothingType();

} // namespace rc
