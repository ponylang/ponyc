#pragma once

#include "rapidcheck/Shrinkable.h"

namespace rc {
namespace shrinkable {

/// Returns true if the given predicate returns true for every shrinkable in
/// the entire tree of the given shrinkable. The tree is searched depth-first.
template <typename T, typename Predicate>
bool all(const Shrinkable<T> &shrinkable, Predicate predicate);

/// Finds a local minimum that satisfies the given predicate. Returns a pair of
/// the minimum value and the number of acceptable values encountered on the way
/// there.
template <typename Predicate, typename T>
std::pair<T, std::size_t> findLocalMin(const Shrinkable<T> &shrinkable,
                                       Predicate pred);

/// Returns a `Seq` of the immediate shrink values.
template <typename T>
Seq<T> immediateShrinks(const Shrinkable<T> &shrinkable);

/// Follows the given path in a shrinkable tree and retuns the `Shrinkable` at
/// the end of it. The path is a vector of integers where each integer is the
/// index of the shrink to walk. For example `[3, 4]` denotes taking the third
/// shrink and then the fourth shrink of that. `[]` denotes the shrinkable that
/// was passed in.
///
/// @return The shrinkable at the end of the path or `Nothing` if the path is
///         not valid for the given shrinkable tree.
template <typename T>
Maybe<Shrinkable<T>> walkPath(const Shrinkable<T> &shrinkable,
                              const std::vector<std::size_t> &path);

} // namespace shrinkable
} // namespace rc

#include "Operations.hpp"
