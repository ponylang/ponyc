#include "LogTestListener.h"

#include "rapidcheck/detail/Property.h"

namespace rc {
namespace detail {

LogTestListener::LogTestListener(std::ostream &os,
                                 bool verboseProgress,
                                 bool verboseShrinking)
    : m_verboseProgress(verboseProgress)
    , m_verboseShrinking(verboseShrinking)
    , m_out(os) {}

void LogTestListener::onTestCaseFinished(const CaseDescription &description) {
  if (!m_verboseProgress) {
    return;
  }

  switch (description.result.type) {
  case CaseResult::Type::Success:
    m_out << ".";
    break;
  case CaseResult::Type::Discard:
    m_out << "x";
    break;
  case CaseResult::Type::Failure:
    m_out << std::endl << "Found failure, shrinking";
    m_out << (m_verboseShrinking ? ":" : "...") << std::endl;
    break;
  }
}

void LogTestListener::onShrinkTried(const CaseDescription &/*shrink*/,
                                    bool accepted) {
  if (!m_verboseShrinking) {
    return;
  }

  if (accepted) {
    m_out << "!";
  } else {
    m_out << ".";
  }
}

void LogTestListener::onTestFinished(const TestMetadata &/*metadata*/,
                                     const TestResult &/*result*/) {
  if (m_verboseShrinking || m_verboseProgress) {
    m_out << std::endl;
  }
}

} // namespace detail
} // namespace rc
