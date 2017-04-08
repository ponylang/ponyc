#include "Testing.h"

#include "rapidcheck/BeforeMinimalTestCase.h"
#include "rapidcheck/shrinkable/Operations.h"

namespace rc {
namespace detail {
namespace {

int sizeFor(const TestParams &params, int i) {
  // We want sizes to be evenly spread, even when maxSuccess is not an even
  // multiple of the number of sizes (i.e. maxSize + 1). Another thing is that
  // we always want to ensure that the maximum size is actually used.

  const auto numSizes = params.maxSize + 1;
  const auto numRegular = (params.maxSuccess / numSizes) * numSizes;
  if (i < numRegular) {
    return i % numSizes;
  }

  const auto numRest = params.maxSuccess - numRegular;
  if (numRest == 1) {
    return 0;
  } else {
    return ((i % numSizes) * params.maxSize) / (numRest - 1);
  }
}

} // namespace

SearchResult searchProperty(const Property &property,
                            const TestParams &params,
                            TestListener &listener) {
  SearchResult searchResult;
  searchResult.type = SearchResult::Type::Success;
  searchResult.numSuccess = 0;
  searchResult.numDiscarded = 0;
  searchResult.tags.reserve(params.maxSuccess);

  const auto maxDiscard = params.maxDiscardRatio * params.maxSuccess;

  auto recentDiscards = 0;
  auto r = Random(params.seed);
  while (searchResult.numSuccess < params.maxSuccess) {
    const auto size =
        sizeFor(params, searchResult.numSuccess) + (recentDiscards / 10);
    const auto random = r.split();

    auto shrinkable = property(random, size);
    auto caseDescription = shrinkable.value();
    listener.onTestCaseFinished(caseDescription);
    const auto &result = caseDescription.result;

    switch (result.type) {
    case CaseResult::Type::Failure:
      searchResult.type = SearchResult::Type::Failure;
      searchResult.failure =
          SearchResult::Failure(std::move(shrinkable), size, random);
      return searchResult;

    case CaseResult::Type::Discard:
      searchResult.numDiscarded++;
      recentDiscards++;
      if (searchResult.numDiscarded > maxDiscard) {
        searchResult.type = SearchResult::Type::GaveUp;
        searchResult.failure =
            SearchResult::Failure(std::move(shrinkable), size, random);
        return searchResult;
      }
      break;

    case CaseResult::Type::Success:
      searchResult.numSuccess++;
      recentDiscards = 0;
      if (!caseDescription.tags.empty()) {
        searchResult.tags.push_back(std::move(caseDescription.tags));
      }
      break;
    }
  }

  return searchResult;
}

std::pair<Shrinkable<CaseDescription>, std::vector<std::size_t>>
shrinkTestCase(const Shrinkable<CaseDescription> &shrinkable,
               TestListener &listener) {
  std::vector<std::size_t> path;
  Shrinkable<CaseDescription> best = shrinkable;

  auto shrinks = shrinkable.shrinks();
  std::size_t i = 0;
  while (auto shrink = shrinks.next()) {
    auto caseDescription = shrink->value();
    bool accept = caseDescription.result.type == CaseResult::Type::Failure;
    listener.onShrinkTried(caseDescription, accept);
    if (accept) {
      best = std::move(*shrink);
      shrinks = best.shrinks();
      path.push_back(i);
      i = 0;
    } else {
      i++;
    }
  }

  return std::make_pair(std::move(best), std::move(path));
}

namespace {

TestResult doTestProperty(const Property &property,
                          const TestParams &params,
                          TestListener &listener) {
  const auto searchResult = searchProperty(property, params, listener);
  if (searchResult.type == SearchResult::Type::Success) {
    SuccessResult success;
    success.numSuccess = searchResult.numSuccess;
    for (const auto &tags : searchResult.tags) {
      success.distribution[tags]++;
    }
    return success;
  } else if (searchResult.type == SearchResult::Type::GaveUp) {
    GaveUpResult gaveUp;
    gaveUp.numSuccess = searchResult.numSuccess;
    const auto &shrinkable = searchResult.failure->shrinkable;
    gaveUp.description = shrinkable.value().result.description;
    return gaveUp;
  } else {
    // Shrink it unless shrinking is disabled
    const auto &shrinkable = searchResult.failure->shrinkable;
    auto shrinkResult = params.disableShrinking
        ? std::make_pair(shrinkable, std::vector<std::size_t>())
        : shrinkTestCase(shrinkable, listener);

    // Give the developer a chance to set a breakpoint before the final minimal
    // test case is run
    beforeMinimalTestCase();
    // ...and here we actually run it
    const auto caseDescription = shrinkResult.first.value();

    FailureResult failure;
    failure.numSuccess = searchResult.numSuccess;
    failure.description = std::move(caseDescription.result.description);
    failure.reproduce.random = searchResult.failure->random;
    failure.reproduce.size = searchResult.failure->size;
    failure.reproduce.shrinkPath = std::move(shrinkResult.second);
    failure.counterExample = caseDescription.example();
    return failure;
  }
}

} // namespace

TestResult testProperty(const Property &property,
                        const TestMetadata &metadata,
                        const TestParams &params,
                        TestListener &listener) {
  TestResult result = doTestProperty(property, params, listener);
  listener.onTestFinished(metadata, result);
  return result;
}

TestResult reproduceProperty(const Property &property,
                             const Reproduce &reproduce) {
  const auto shrinkable = property(reproduce.random, reproduce.size);
  const auto minShrinkable =
      shrinkable::walkPath(shrinkable, reproduce.shrinkPath);
  if (!minShrinkable) {
    return Error("Unable to reproduce minimum value");
  }

  // Give the developer a chance to set a breakpoint before the final minimal
  // test case is run
  beforeMinimalTestCase();
  // ...and here we actually run it
  auto desc = minShrinkable->value();
  if (desc.result.type != CaseResult::Type::Failure) {
    return Error("Reproduced result is not a failure");
  }

  FailureResult failure;
  failure.numSuccess = 0;
  failure.description = std::move(desc.result.description);
  failure.reproduce = reproduce;
  failure.counterExample = desc.example();
  return failure;
}

} // namespace detail
} // namespace rc
