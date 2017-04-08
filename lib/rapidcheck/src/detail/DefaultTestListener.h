#pragma once

#include <memory>

#include "rapidcheck/detail/TestListener.h"
#include "rapidcheck/detail/Configuration.h"

namespace rc {
namespace detail {

/// Creates a default `TestListener`.
///
/// @param config  The configuration describing the listener.
/// @param os      The output stream to print information to.
std::unique_ptr<TestListener>
makeDefaultTestListener(const Configuration &config, std::ostream &os);

/// Returns the global default `TestListener`.
TestListener &globalTestListener();

} // namespace detail
} // namespace rc
