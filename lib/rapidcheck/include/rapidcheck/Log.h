#pragma once

#include <iostream>

/// Logs additional information about the run of a test case. Can be used either
/// like a stream (`RC_LOG() << "foobar"`) or taking a string
/// (`RC_LOG("foobar")`). When using the latter form, a newline will be appended
/// automatically.
#define RC_LOG(...) ::rc::detail::log(__VA_ARGS__)

#include "Log.hpp"
