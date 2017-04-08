#pragma once

#include "rapidcheck/Maybe.h"

#if defined(__GNUC__)
#define RC_INTERNAL_DEPRECATED(msg) __attribute__((deprecated(msg)))
#elif defined(_MSC_VER)
#define RC_INTERNAL_DEPRECATED(msg) __declspec(deprecated(msg))
#else
#pragma message(                                                        \
    "You need to implement RC_INTERNAL_DEPRECATED for this compiler")
#endif

namespace rc {
namespace detail {

/// Demangles a mangled C++ name.
std::string demangle(const char *name);

/// Returns the value of the environment variable with the given name or
/// `Nothing` if it is not set.
Maybe<std::string> getEnvValue(const std::string &name);

} // namespace detail
} // namespace rc
