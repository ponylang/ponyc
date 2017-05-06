#pragma once

#include <cassert>

#include "rapidcheck/Traits.h"
#include "rapidcheck/Show.h"
#include "Utility.h"
#include "ShowType.h"

namespace rc {
namespace detail {

class Any::IAnyImpl {
public:
  virtual void *get() = 0;
  virtual void showType(std::ostream &os) const = 0;
  virtual void showValue(std::ostream &os) const = 0;
  virtual const std::type_info &typeInfo() const = 0;
  virtual ~IAnyImpl() = default;
};

template <typename T>
class Any::AnyImpl : public Any::IAnyImpl {
public:
  template <typename ValueT>
  AnyImpl(ValueT &&value)
      : m_value(std::forward<ValueT>(value)) {}

  void *get() override { return &m_value; }

  void showType(std::ostream &os) const override { rc::detail::showType<T>(os); }

  void showValue(std::ostream &os) const override { show(m_value, os); }

  const std::type_info &typeInfo() const override { return typeid(T); }

private:
  T m_value;
};

/// Constructs a new `Any` with the given value.
template <typename T>
Any Any::of(T &&value) {
  Any any;
  any.m_impl.reset(new AnyImpl<Decay<T>>(std::forward<T>(value)));
  return any;
}

template <typename T>
const T &Any::get() const {
  assert(m_impl);
  assert(m_impl->typeInfo() == typeid(T));
  return *static_cast<T *>(m_impl->get());
}

template <typename T>
T &Any::get() {
  assert(m_impl);
  assert(m_impl->typeInfo() == typeid(T));
  return *static_cast<T *>(m_impl->get());
}

} // namespace detail
} // namespace rc
