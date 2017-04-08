#include "ReproduceListener.h"

#include "StringSerialization.h"

namespace rc {
namespace detail {

ReproduceListener::ReproduceListener(std::ostream &os)
    : m_out(os) {}

void ReproduceListener::onTestFinished(const TestMetadata &metadata,
                                       const TestResult &result) {
  if (metadata.id.empty()) {
    return;
  }

  FailureResult failure;
  if (result.match(failure)) {
    m_reproduceMap.emplace(metadata.id, failure.reproduce);
  }
}

ReproduceListener::~ReproduceListener() {
  if (m_reproduceMap.empty()) {
    return;
  }

  m_out << "Some of your RapidCheck properties had failures. To "
        << "reproduce these, run with:" << std::endl
        << "RC_PARAMS=\"reproduce=" << reproduceMapToString(m_reproduceMap)
        << "\"" << std::endl;
}

} // namespace detail
} // namespace rc
