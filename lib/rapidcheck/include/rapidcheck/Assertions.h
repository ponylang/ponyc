#pragma once

#include "rapidcheck/detail/Results.h"
#include "rapidcheck/detail/Capture.h"

#define RC_INTERNAL_CONDITIONAL_RESULT(                                        \
    ResultType, expression, invert, name, ...)                                 \
  doAssert(RC_INTERNAL_CAPTURE(expression),                                    \
           (invert),                                                           \
           ::rc::detail::CaseResult::Type::ResultType,                         \
           __FILE__,                                                           \
           __LINE__,                                                           \
           name "(" #expression ")")

#define RC_INTERNAL_STRINGIFY(x) #x

#define RC_INTERNAL_UNCONDITIONAL_RESULT(ResultType, name, expression)         \
  do {                                                                         \
    throw ::rc::detail::CaseResult(                                            \
        ::rc::detail::CaseResult::Type::ResultType,                            \
        ::rc::detail::makeMessage(                                             \
            __FILE__, __LINE__, name "(" #expression ")", {expression}));      \
  } while (false)

/// Fails the current test case unless the given condition is `true`.
#define RC_ASSERT(expression)                                                  \
  RC_INTERNAL_CONDITIONAL_RESULT(Failure, expression, true, "RC_ASSERT", )

/// Fails the current test case unless the given condition is `false`.
#define RC_ASSERT_FALSE(expression)                                            \
  RC_INTERNAL_CONDITIONAL_RESULT(Failure,                                      \
                                 expression,                                   \
                                 false,                                        \
                                 "RC_ASSERT_"                                  \
                                 "FALSE")

/// Fails the current test case unless the provided expression throws an
/// exception of any type
#define RC_ASSERT_THROWS(expression)                                           \
  do {                                                                         \
    try {                                                                      \
      expression;                                                              \
    } catch (...) {                                                            \
      break;                                                                   \
    }                                                                          \
    throw ::rc::detail::CaseResult(                                            \
        ::rc::detail::CaseResult::Type::Failure,                               \
        ::rc::detail::makeUnthrownExceptionMessage(                            \
            __FILE__, __LINE__, "RC_ASSERT_THROWS(" #expression ")"));         \
  } while (false)

/// Fails the current test case unless the given expression throws an
/// exception that matches the given exception type
#define RC_ASSERT_THROWS_AS(expression, ExceptionType)                         \
  do {                                                                         \
    try {                                                                      \
      expression;                                                              \
    } catch (const ExceptionType &) {                                          \
      break;                                                                   \
    } catch (...) {                                                            \
      throw ::rc::detail::CaseResult(::rc::detail::CaseResult::Type::Failure,  \
                                     ::rc::detail::makeWrongExceptionMessage(  \
                                         __FILE__,                             \
                                         __LINE__,                             \
                                         "RC_ASSERT_THROWS_AS(" #expression    \
                                         ", " #ExceptionType ")",              \
                                         #ExceptionType));                     \
    }                                                                          \
    throw ::rc::detail::CaseResult(::rc::detail::CaseResult::Type::Failure,    \
                                   ::rc::detail::makeUnthrownExceptionMessage( \
                                       __FILE__,                               \
                                       __LINE__,                               \
                                       "RC_ASSERT_THROWS_AS(" #expression      \
                                       ", " #ExceptionType ")"));              \
  } while (false)

/// Unconditionally fails the current test case with the given message.
#define RC_FAIL(...)                                                           \
  RC_INTERNAL_UNCONDITIONAL_RESULT(Failure, "RC_FAIL", __VA_ARGS__)

/// Succeed if the given condition is true.
#define RC_SUCCEED_IF(expression)                                              \
  RC_INTERNAL_CONDITIONAL_RESULT(Success, expression, false, "RC_SUCCEED_IF")

/// Unconditionally succeed with the given message.
#define RC_SUCCEED(...)                                                        \
  RC_INTERNAL_UNCONDITIONAL_RESULT(Success, "RC_SUCCEED", __VA_ARGS__)

/// Discards the current test case if the given condition is false.
#define RC_PRE(expression)                                                     \
  RC_INTERNAL_CONDITIONAL_RESULT(Discard, expression, true, "RC_PRE", !)

/// Discards the current test case with the given description.
#define RC_DISCARD(...)                                                        \
  RC_INTERNAL_UNCONDITIONAL_RESULT(Discard, "RC_DISCARD", __VA_ARGS__)

#include "Assertions.hpp"
