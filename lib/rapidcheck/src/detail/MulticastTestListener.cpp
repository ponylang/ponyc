#include "MulticastTestListener.h"

namespace rc {
namespace detail {

MulticastTestListener::MulticastTestListener(Listeners listeners)
    : m_listeners(std::move(listeners)) {}

void MulticastTestListener::onTestCaseFinished(
    const CaseDescription &description) {
  for (auto &listener : m_listeners) {
    listener->onTestCaseFinished(description);
  }
}

void MulticastTestListener::onShrinkTried(const CaseDescription &shrink,
                                          bool accepted) {
  for (auto &listener : m_listeners) {
    listener->onShrinkTried(shrink, accepted);
  }
}

void MulticastTestListener::onTestFinished(const TestMetadata &metadata,
                                           const TestResult &result) {
  for (auto &listener : m_listeners) {
    listener->onTestFinished(metadata, result);
  }
}

} // namespace detail
} // namespace rc
