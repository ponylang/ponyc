#pragma once

namespace rc {
namespace gen {
namespace detail {

template <typename Iterator>
class ShrinkValueIterator;

/// Wraps an iterator that iterates over `Shrinkable`s and makes it returns the
/// value of the shrinkables instead.
template <typename Iterator>
ShrinkValueIterator<Iterator> makeShrinkValueIterator(Iterator it);

} // namespace detail
} // namespace gen
} // namespace rc

#include "ShrinkValueIterator.hpp"
