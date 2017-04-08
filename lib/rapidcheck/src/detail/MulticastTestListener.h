#pragma once

#include "rapidcheck/detail/TestListener.h"

namespace rc {
namespace detail {

/// Listener which broadcasts to other listeners.
class MulticastTestListener : public TestListener {
public:
  using Listeners = std::vector<std::unique_ptr<TestListener>>;

  explicit MulticastTestListener(Listeners listeners);
  void onTestCaseFinished(const CaseDescription &description) override;
  void onShrinkTried(const CaseDescription &shrink, bool accepted) override;
  void onTestFinished(const TestMetadata &metadata,
                      const TestResult &result) override;

private:
  Listeners m_listeners;
};

} // namespace detail
} // namespace rc
