#pragma once

namespace rc {
namespace detail {

/**
 * Outputs a string representation of `T` to `os`.
 */
template <typename T>
void showType(std::ostream &os);

/**
 * Returns a string representation of type `T`.
 */
template <typename T>
std::string typeToString();

} // namespace detail
} // namespace rc

#include "ShowType.hpp"
