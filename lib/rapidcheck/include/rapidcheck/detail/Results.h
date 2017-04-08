#pragma once

#include <vector>
#include <map>

#include "rapidcheck/Random.h"
#include "rapidcheck/detail/Variant.h"

namespace rc {
namespace detail {

using Example = std::vector<std::pair<std::string, std::string>>;
using Tags = std::vector<std::string>;
using Distribution = std::map<Tags, int>;

/// Describes the result of a test case.
struct CaseResult {
  /// Enum for the type of the result.
  enum class Type {
    /// The test case succeeded.
    Success,
    /// The test case failed.
    Failure,
    /// The preconditions for the test case were not met.
    Discard
  };

  CaseResult();
  CaseResult(Type t, std::string desc = "");

  /// The type of the result.
  Type type;

  /// A description of the result.
  std::string description;
};

std::ostream &operator<<(std::ostream &os, CaseResult::Type type);
std::ostream &operator<<(std::ostream &os, const CaseResult &result);
bool operator==(const CaseResult &r1, const CaseResult &r2);
bool operator!=(const CaseResult &r1, const CaseResult &r2);

/// Contains all that is required to reproduce a failing test case.
struct Reproduce {
  /// The Random to generate the shrinkable with.
  Random random;
  /// The size to generate the shrinkable with.
  int size;
  /// The shrink path to follow.
  std::vector<std::size_t> shrinkPath;
};

std::ostream &operator<<(std::ostream &os, const detail::Reproduce &r);
bool operator==(const Reproduce &lhs, const Reproduce &rhs);
bool operator!=(const Reproduce &lhs, const Reproduce &rhs);

template <typename Iterator>
Iterator serialize(const Reproduce &value, Iterator output);
template <typename Iterator>
Iterator deserialize(Iterator begin, Iterator end, Reproduce &out);

/// Indicates a successful property.
struct SuccessResult {
  /// The number of successful tests run.
  int numSuccess;
  /// The test case distribution. This is a map from tags to count.
  Distribution distribution;
};

std::ostream &operator<<(std::ostream &os, const detail::SuccessResult &result);
bool operator==(const SuccessResult &r1, const SuccessResult &r2);
bool operator!=(const SuccessResult &r1, const SuccessResult &r2);

/// Indicates that a property failed.
struct FailureResult {
  /// The number of successful tests run.
  int numSuccess;
  /// A description of the failure.
  std::string description;
  /// The information required to reproduce the failure.
  Reproduce reproduce;
  /// The counterexample.
  Example counterExample;
};

std::ostream &operator<<(std::ostream &os, const detail::FailureResult &result);
bool operator==(const FailureResult &r1, const FailureResult &r2);
bool operator!=(const FailureResult &r1, const FailureResult &r2);

/// Indicates that more test cases than allowed were discarded.
struct GaveUpResult {
  /// The number of successful tests run.
  int numSuccess;
  /// A description of the reason for giving up.
  std::string description;
};

std::ostream &operator<<(std::ostream &os, const detail::GaveUpResult &result);
bool operator==(const GaveUpResult &r1, const GaveUpResult &r2);
bool operator!=(const GaveUpResult &r1, const GaveUpResult &r2);

/// Indicates that the testing process itself failed.
struct Error {
  Error() = default;
  Error(std::string desc);

  /// A description of the error.
  std::string description;
};

std::ostream &operator<<(std::ostream &os, const detail::Error &result);
bool operator==(const Error &lhs, const Error &rhs);
bool operator!=(const Error &lhs, const Error &rhs);

/// Describes the circumstances around the result of a test.
using TestResult = Variant<SuccessResult, FailureResult, GaveUpResult, Error>;

/// Prints a human readable error message describing the given success result to
/// the specified stream.
void printResultMessage(const SuccessResult &result, std::ostream &os);

/// Prints a human readable error message describing the given failure result to
/// the specified stream.
void printResultMessage(const FailureResult &result, std::ostream &os);

/// Prints a human readable error message describing the given gave-up result to
/// the specified stream.
void printResultMessage(const GaveUpResult &result, std::ostream &os);

/// Prints a short message describing the given test results to the specified
/// stream.
void printResultMessage(const TestResult &result, std::ostream &os);

} // namespace detail
} // namespace rc

#include "Results.hpp"
