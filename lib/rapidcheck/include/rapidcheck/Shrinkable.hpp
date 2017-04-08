#pragma once

#include <atomic>

namespace rc {

template <typename T>
class Shrinkable<T>::IShrinkableImpl {
public:
  virtual T value() const = 0;
  virtual Seq<Shrinkable<T>> shrinks() const = 0;
  virtual void retain() = 0;
  virtual void release() = 0;
  virtual ~IShrinkableImpl() = default;
};

template <typename T>
template <typename Impl>
class Shrinkable<T>::ShrinkableImpl : public IShrinkableImpl {
public:
  template <typename... Args>
  explicit ShrinkableImpl(Args &&... args)
      : m_impl(std::forward<Args>(args)...)
      , m_count(1) {}

  T value() const override { return m_impl.value(); }
  Seq<Shrinkable<T>> shrinks() const override { return m_impl.shrinks(); }

  void retain() override { m_count.fetch_add(1L); }

  void release() override {
    if (m_count.fetch_sub(1L) == 1L) {
      delete this;
    }
  }

private:
  const Impl m_impl;
  std::atomic<long> m_count;
};

template <typename T>
T Shrinkable<T>::value() const {
  return m_impl->value();
}

template <typename T>
Seq<Shrinkable<T>> Shrinkable<T>::shrinks() const noexcept {
  try {
    return m_impl->shrinks();
  } catch (...) {
    return Seq<Shrinkable<T>>();
  }
}

template <typename T>
Shrinkable<T>::Shrinkable(const Shrinkable &other) noexcept
    : m_impl(other.m_impl) {
  m_impl->retain();
}

template <typename T>
Shrinkable<T>::Shrinkable(Shrinkable &&other) noexcept : m_impl(other.m_impl) {
  other.m_impl = nullptr;
}

template <typename T>
Shrinkable<T> &Shrinkable<T>::operator=(const Shrinkable &other) noexcept {
  other.m_impl->retain();
  if (m_impl) {
    m_impl->release();
  }
  m_impl = other.m_impl;
  return *this;
}

template <typename T>
Shrinkable<T> &Shrinkable<T>::operator=(Shrinkable &&other) noexcept {
  if (m_impl) {
    m_impl->release();
  }
  m_impl = other.m_impl;
  other.m_impl = nullptr;
  return *this;
}

template <typename T>
Shrinkable<T>::~Shrinkable() noexcept {
  if (m_impl) {
    m_impl->release();
  }
}

template <typename Impl, typename... Args>
Shrinkable<Decay<decltype(std::declval<Impl>().value())>>
makeShrinkable(Args &&... args) {
  using T = decltype(std::declval<Impl>().value());
  using ShrinkableImpl = typename Shrinkable<T>::template ShrinkableImpl<Impl>;

  Shrinkable<T> shrinkable;
  shrinkable.m_impl = new ShrinkableImpl(std::forward<Args>(args)...);
  return shrinkable;
}

template <typename T>
bool operator==(const Shrinkable<T> &lhs, const Shrinkable<T> &rhs) {
  return (lhs.value() == rhs.value()) && (lhs.shrinks() == rhs.shrinks());
}

template <typename T>
bool operator!=(const Shrinkable<T> &lhs, const Shrinkable<T> &rhs) {
  return !(lhs == rhs);
}

} // namespace rc
