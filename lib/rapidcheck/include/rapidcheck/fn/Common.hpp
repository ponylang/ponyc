#pragma once

namespace rc {
namespace fn {

template <typename T>
class Constant {
public:
  template <typename Arg,
            typename = typename std::enable_if<
                !std::is_same<Decay<Arg>, Constant>::value>::type>
  explicit Constant(Arg &&arg)
      : m_value(std::forward<Arg>(arg)) {}

  template <typename... Args>
  T operator()(Args &&... /*args*/) const {
    return m_value;
  }

private:
  T m_value;
};

template <typename T>
Constant<Decay<T>> constant(T &&value) {
  return Constant<Decay<T>>(std::forward<T>(value));
}

} // namespace fn
} // namespace rc
