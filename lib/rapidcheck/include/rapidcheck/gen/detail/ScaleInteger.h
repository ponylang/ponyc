#pragma once

#include <cstdint>

namespace rc {
namespace gen {
namespace detail {

/// Scales a 64-bit integer according to `size` without overflows. `size` maxes
/// out at `100`.
uint64_t scaleInteger(uint64_t rangeSize, int size);

} // namespace detail
} // namespace gen
} // namespace rc
