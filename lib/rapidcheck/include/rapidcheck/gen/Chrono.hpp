#pragma once

namespace rc {
namespace gen {
namespace detail {

template <typename Rep, typename Period>
struct DefaultArbitrary<std::chrono::duration<Rep, Period>> {
  static Gen<std::chrono::duration<Rep, Period>> arbitrary() {
    return gen::map<Rep>(
        [](Rep rep) { return std::chrono::duration<Rep, Period>(rep); });
  }
};

template <typename Clock, typename Duration>
struct DefaultArbitrary<std::chrono::time_point<Clock, Duration>> {
  static Gen<std::chrono::time_point<Clock, Duration>> arbitrary() {
    return gen::map<Duration>([](Duration duration) {
      return std::chrono::time_point<Clock, Duration>(duration);
    });
  }
};

} // namespace detail
} // namespace gen
} // namespace rc
