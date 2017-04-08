#pragma once

#include "rapidcheck/detail/TestListenerAdapter.h"

namespace rc {
namespace detail {

/// `TestListener` that listens for test results and collects `Reproduce` value
/// that can be supplied to the user when the tests have finished running.
/// Prints stuff on destruction.
class ReproduceListener : public TestListenerAdapter {
public:
  explicit ReproduceListener(std::ostream &os);
  void onTestFinished(const TestMetadata &metadata,
                      const TestResult &result) override;
  ~ReproduceListener();

private:
  std::unordered_map<std::string, Reproduce> m_reproduceMap;
  std::ostream &m_out;
};

} // namespace detail
} // namespace rc
