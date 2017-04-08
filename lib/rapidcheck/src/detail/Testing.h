#pragma once

#include <vector>

#include "rapidcheck/detail/Results.h"
#include "rapidcheck/Maybe.h"
#include "rapidcheck/Shrinkable.h"
#include "rapidcheck/detail/Property.h"
#include "rapidcheck/detail/TestParams.h"
#include "rapidcheck/detail/TestListener.h"

namespace rc {
namespace detail {

struct SearchResult {
  enum class Type { Success, Failure, GaveUp };

  /// Represents information about a failure.
  struct Failure {
    Failure(Shrinkable<CaseDescription> shr, int sz, const Random &rnd)
        : shrinkable(shr)
        , size(sz)
        , random(rnd) {}

    /// The shrinkable of the failing test case.
    Shrinkable<CaseDescription> shrinkable;

    /// The size at which the property failed.
    int size;

    /// The Random state which produced the failure.
    Random random;
  };

  /// The type of the result.
  Type type;

  /// The number of successful test cases.
  int numSuccess;

  /// The number of discarded test cases.
  int numDiscarded;

  /// The tags of successful test cases.
  std::vector<Tags> tags;

  /// On Failure or GiveUp, contains failure information.
  Maybe<Failure> failure;
};

/// Searches for a failure in the given property.
///
/// @param property  The property to search.
/// @param params    The test parameters to use.
/// @param listener  Listener that will receive callbacks on test progress.
///
/// @return A `SearchResult` structure describing the result of the search.
SearchResult searchProperty(const Property &property,
                            const TestParams &params,
                            TestListener &listener);

/// Shrinks the given case description shrinkable.
///
/// @param shrinkable  The shrinkable to shrink.
/// @param listener    A test listener to report progress to.
///
/// @return A pair of the final shrink as well as the path leading there.
std::pair<Shrinkable<CaseDescription>, std::vector<std::size_t>>
shrinkTestCase(const Shrinkable<CaseDescription> &shrinkable,
               TestListener &listener);

/// Combined search and shrink. Returns a test result.
///
/// @param property  The property to test.
/// @param metadata  Metadata about the test.
/// @param params    The test parameters.
/// @param listener  A test listener to report progress to.
TestResult testProperty(const Property &property,
                        const TestMetadata &metadata,
                        const TestParams &params,
                        TestListener &listener);

/// Reproduces a test result for the given property using a `Reproduce` value.
TestResult reproduceProperty(const Property &property,
                             const Reproduce &reproduce);

} // namespace detail
} // namespace rc
