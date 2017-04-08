#include "DefaultTestListener.h"

#include "LogTestListener.h"
#include "ReproduceListener.h"
#include "MulticastTestListener.h"

namespace rc {
namespace detail {

namespace {

template <typename T, typename... Args>
std::unique_ptr<TestListener> makeListener(Args &&... args) {
  return std::unique_ptr<TestListener>(new T(std::forward<Args>(args)...));
}

} // namespace

std::unique_ptr<TestListener>
makeDefaultTestListener(const Configuration &config, std::ostream &os) {
  MulticastTestListener::Listeners listeners;
  listeners.push_back(makeListener<LogTestListener>(
      os, config.verboseProgress, config.verboseShrinking));
  listeners.push_back(makeListener<ReproduceListener>(os));
  return makeListener<MulticastTestListener>(std::move(listeners));
}

TestListener &globalTestListener() {
  static const auto listener =
      makeDefaultTestListener(configuration(), std::cerr);
  return *listener;
}

} // namespace detail
} // namespace rc
