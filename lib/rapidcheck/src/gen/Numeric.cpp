#include "rapidcheck/gen/Numeric.h"

namespace rc {
namespace gen {
namespace detail {

template Shrinkable<char> integral<char>(const Random &random, int size);
template Shrinkable<unsigned char> integral<unsigned char>(const Random &random,
                                                           int size);
template Shrinkable<short> integral<short>(const Random &random, int size);
template Shrinkable<unsigned short>
integral<unsigned short>(const Random &random, int size);
template Shrinkable<int> integral<int>(const Random &random, int size);
template Shrinkable<unsigned int> integral<unsigned int>(const Random &random,
                                                         int size);
template Shrinkable<long> integral<long>(const Random &random, int size);
template Shrinkable<unsigned long> integral<unsigned long>(const Random &random,
                                                           int size);
template Shrinkable<long long> integral<long long>(const Random &random,
                                                   int size);
template Shrinkable<unsigned long long>
integral<unsigned long long>(const Random &random, int size);

template Shrinkable<float> real<float>(const Random &random, int size);
template Shrinkable<double> real<double>(const Random &random, int size);

Shrinkable<bool> boolean(const Random &random, int /*size*/) {
  return shrinkable::shrinkRecur(rc::detail::bitStreamOf(random).next<bool>(),
                                 &shrink::boolean);
}

} // namespace detail
} // namespace gen
} // namespace rc
