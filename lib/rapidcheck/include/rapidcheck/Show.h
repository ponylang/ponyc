#pragma once

#include <iostream>
#include <string>

namespace rc {

/// Outputs a human readable representation of the given value to the given
/// output stream. To do this, it tries the following methods in order until one
/// works:
///
/// 1. Use a suitable overload of `void showValue(T, std::ostream)´
/// 2. Use a suitable overload of `std::ostream &operator<<(...)`
/// 3. Output a placeholder value.
template <typename T>
void show(const T &value, std::ostream &os);

/// Uses show(...) to convert argument to a string.
template <typename T>
std::string toString(const T &value);

/// Helper function for showing collections of values.
///
/// @param prefix      The prefix to the collection, for example "["
/// @param suffix      The suffix to the collection, for example "]"
/// @param collection  The collection type. Must support `begin()` and `end()`.
/// @param os          The stream to output to.
template <typename Collection>
void showCollection(const std::string &prefix,
                    const std::string &suffix,
                    const Collection &collection,
                    std::ostream &os);

} // namespace rc

#include "Show.hpp"
