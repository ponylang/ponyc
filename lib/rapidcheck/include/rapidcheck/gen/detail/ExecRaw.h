#pragma once

#include "rapidcheck/detail/FunctionTraits.h"
#include "rapidcheck/gen/detail/Recipe.h"

namespace rc {
namespace gen {
namespace detail {

/// "Raw" version of `gen::exec` (which uses this generator) that both return
/// the generated value and the `Recipe` used to do so.
template <typename Callable>
Gen<std::pair<rc::detail::ReturnType<Callable>, Recipe>>
execRaw(Callable callable);

} // namespace detail
} // namespace gen
} // namespace rc

#include "ExecRaw.hpp"
