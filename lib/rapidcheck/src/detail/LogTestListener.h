#pragma once

#include <iostream>

#include "rapidcheck/detail/TestListener.h"

namespace rc {
namespace detail {

/// Listener which prints info to an `std::ostream`.
class LogTestListener : public TestListener {
public:
  explicit LogTestListener(std::ostream &os,
                           bool verboseProgress = false,
                           bool verboseShrinking = false);
  void onTestCaseFinished(const CaseDescription &description) override;
  void onShrinkTried(const CaseDescription &shrink, bool accepted) override;
  void onTestFinished(const TestMetadata &metadata,
                      const TestResult &result) override;

private:
  bool m_verboseProgress;
  bool m_verboseShrinking;
  std::ostream &m_out;
};

} // namespace detail
} // namespace rc
