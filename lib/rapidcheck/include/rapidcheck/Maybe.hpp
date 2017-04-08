#pragma once

#include "rapidcheck/Show.h"

namespace rc {

template <typename T>
Maybe<T>::Maybe() noexcept : m_initialized(false) {}

template <typename T>
Maybe<T>::Maybe(NothingType) noexcept : Maybe() {}

template <typename T>
Maybe<T>::Maybe(const T &value)
    : m_initialized(false) {
  init(value);
}

template <typename T>
Maybe<T>::Maybe(T &&value)
    : m_initialized(false) {
  init(std::move(value));
}

template <typename T>
Maybe<T> &Maybe<T>::operator=(const T &value) {
  if (m_initialized) {
    **this = value;
  } else {
    init(value);
  }
  return *this;
}

template <typename T>
Maybe<T> &Maybe<T>::operator=(T &&value) {
  if (m_initialized) {
    **this = std::move(value);
  } else {
    init(std::move(value));
  }
  return *this;
}

template <typename T>
Maybe<T> &Maybe<T>::operator=(NothingType) {
  reset();
  return *this;
}

template <typename T>
template <typename... Args>
void Maybe<T>::init(Args &&... args) {
  reset();
  new (&m_storage) T(std::forward<Args>(args)...);
  m_initialized = true;
}

template <typename T>
void Maybe<T>::reset() {
  if (m_initialized) {
    m_initialized = false;
    (**this).~T();
  }
}

template <typename T>
T &Maybe<T>::operator*() & {
  return *reinterpret_cast<T *>(&m_storage);
}

template <typename T>
const T &Maybe<T>::operator*() const & {
  return *reinterpret_cast<const T *>(&m_storage);
}

template <typename T>
T &&Maybe<T>::operator*() && {
  return std::move(*reinterpret_cast<T *>(&m_storage));
}

template <typename T>
T *Maybe<T>::operator->() {
  return reinterpret_cast<T *>(&m_storage);
}

template <typename T>
const T *Maybe<T>::operator->() const {
  return reinterpret_cast<const T *>(&m_storage);
}

template <typename T>
Maybe<T>::operator bool() const {
  return m_initialized;
}

template <typename T>
Maybe<T>::Maybe(const Maybe &other) noexcept(
    std::is_nothrow_copy_constructible<T>::value)
    : m_initialized(false) {
  if (other.m_initialized) {
    init(*other);
  }
}

template <typename T>
Maybe<T> &Maybe<T>::operator=(const Maybe &rhs) noexcept(
    std::is_nothrow_copy_constructible<T>::value
        &&std::is_nothrow_copy_assignable<T>::value) {
  if (rhs.m_initialized) {
    *this = *rhs;
  } else {
    reset();
  }

  return *this;
}

template <typename T>
Maybe<T>::Maybe(Maybe &&other) noexcept(
    std::is_nothrow_move_constructible<T>::value)
    : m_initialized(false) {
  if (other.m_initialized) {
    init(std::move(*other));
  }
}

template <typename T>
Maybe<T> &Maybe<T>::
operator=(Maybe &&rhs) noexcept(std::is_nothrow_move_constructible<T>::value &&
                                    std::is_nothrow_move_assignable<T>::value) {
  if (rhs.m_initialized) {
    *this = std::move(*rhs);
  } else {
    reset();
  }

  return *this;
}

template <typename T>
Maybe<T>::~Maybe() {
  if (m_initialized) {
    (**this).~T();
  }
}

template <typename T, typename U>
bool operator==(const Maybe<T> &lhs, const Maybe<U> &rhs) {
  if (!lhs && !rhs) {
    return true;
  } else if (lhs && rhs) {
    return *lhs == *rhs;
  }

  return false;
}

template <typename T, typename U>
bool operator!=(const Maybe<T> &lhs, const Maybe<U> &rhs) {
  return !(lhs == rhs);
}

template <typename T>
std::ostream &operator<<(std::ostream &os, const Maybe<T> &value) {
  if (value) {
    show(*value, os);
  } else {
    os << "Nothing";
  }
  return os;
}

} // namespace rc
