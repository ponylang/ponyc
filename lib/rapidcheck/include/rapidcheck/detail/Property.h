#pragma once

#include <functional>

#include "rapidcheck/Gen.h"
#include "rapidcheck/detail/Results.h"

namespace rc {
namespace detail {

struct CaseDescription {
  CaseResult result;
  std::vector<std::string> tags;
  std::function<Example()> example;
};

bool operator==(const CaseDescription &lhs, const CaseDescription &rhs);
bool operator!=(const CaseDescription &lhs, const CaseDescription &rhs);
std::ostream &operator<<(std::ostream &os, const CaseDescription &desc);

using Property = Gen<CaseDescription>;

/// Takes a callable and converts it into a generator of a `CaseResult` and a
/// counterexample. That is, a `Property`.
template <typename Callable>
Property toProperty(Callable &&callable);

} // namespace detail
} // namespace rc

#include "Property.hpp"
