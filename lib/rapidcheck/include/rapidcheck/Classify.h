#pragma once

/// Tags the current test case if the specified condition is true. If any tags
/// are specified after the condition, those will be used. Otherwise, a string
/// version of the condition itself will be used as the tag.
#define RC_CLASSIFY(condition, ...)                                            \
  do {                                                                         \
    if (condition) {                                                           \
      ::rc::detail::classify(#condition, {__VA_ARGS__});                       \
    }                                                                          \
  } while (false)

/// Tags the current test case with the given values which will be converted to
/// strings.
#define RC_TAG(...) ::rc::detail::tag({__VA_ARGS__})

#include "Classify.hpp"
