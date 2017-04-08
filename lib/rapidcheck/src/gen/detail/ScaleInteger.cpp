#include "rapidcheck/gen/detail/ScaleInteger.h"

#include <algorithm>

#include "rapidcheck/Gen.h"

namespace rc {
namespace gen {
namespace detail {
namespace {

// Calculates (a * b) / c, rounded
uint64_t mulDiv(uint64_t a, uint32_t b, uint32_t c) {
  constexpr uint64_t kMask = 0xFFFFFFFFULL;

  const uint64_t ah = a >> 32;
  const uint64_t al = a & kMask;
  const uint64_t bl = b;

  const uint64_t x1 = al * bl;
  const uint64_t x2 = ah * bl;
  uint64_t xh = x2 >> 32;
  uint64_t xl = x2 << 32;
  const uint64_t xl0 = xl;
  xl += x1;
  if (xl < xl0) {
    xh++;
  }

  uint64_t y = 0;
  uint64_t r = 0;

  r |= xh;
  r = (r % c) << 32;

  r |= xl >> 32;
  y |= (r / c) << 32;
  r = (r % c) << 32;

  r |= xl & kMask;
  y |= r / c;
  r = r % c;

  return (r < (c / 2)) ? y : (y + 1);
}

} // namespace

uint64_t scaleInteger(uint64_t x, int size) {
  const auto clampedSize = std::min(kNominalSize, size);
  return mulDiv(x, clampedSize, kNominalSize);
}

} // namespace detail
} // namespace gen
} // namespace rc
