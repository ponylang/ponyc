#pragma once

#include <iostream>

#include "rapidcheck/detail/Configuration.h"
#include "rapidcheck/detail/Results.h"
#include "rapidcheck/detail/Property.h"
#include "rapidcheck/detail/TestListener.h"

namespace rc {
namespace detail {

TestResult
checkProperty(const Property &property,
              const TestMetadata &metadata,
              const TestParams &params,
              TestListener &listener,
              const std::unordered_map<std::string, Reproduce> &reproduceMap);

TestResult checkProperty(const Property &property,
                         const TestMetadata &metadata,
                         const TestParams &params,
                         TestListener &listener);

TestResult checkProperty(const Property &property,
                         const TestMetadata &metadata,
                         const TestParams &params);

TestResult checkProperty(const Property &property,
                         const TestMetadata &metadata);

// Uses defaults from configuration
TestResult checkProperty(const Property &property);

template <typename Testable, typename... Args>
TestResult checkTestable(Testable &&testable, Args &&... args) {
  return checkProperty(toProperty(std::forward<Testable>(testable)),
                       std::forward<Args>(args)...);
}

} // namespace detail

template <typename Testable>
bool check(Testable &&testable) {
  return check(std::string(), std::forward<Testable>(testable));
}

template <typename Testable>
bool check(const std::string &description, Testable &&testable) {
  using namespace rc::detail;

  // Force loading of the configuration so that message comes _before_ the
  // description
  configuration();

  if (!description.empty()) {
    std::cerr << std::endl << "- " << description << std::endl;
  }

  TestMetadata metadata;
  metadata.id = description;
  metadata.description = description;
  const auto result =
      detail::checkTestable(std::forward<Testable>(testable), metadata);

  printResultMessage(result, std::cerr);
  std::cerr << std::endl;

  return result.template is<detail::SuccessResult>();
}

} // namespace rc
