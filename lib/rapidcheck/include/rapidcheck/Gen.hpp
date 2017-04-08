#pragma once

#include <cassert>

#include "rapidcheck/detail/Any.h"
#include "rapidcheck/detail/ImplicitParam.h"
#include "rapidcheck/gen/detail/GenerationHandler.h"
#include "rapidcheck/shrinkable/Create.h"

namespace rc {
namespace gen {

// Forward declare this so we don't need to include Transform.h
template <typename T, typename Mapper>
Gen<Decay<typename std::result_of<Mapper(T)>::type>> map(Gen<T> gen,
                                                         Mapper &&mapper);

} // namespace gen

template <typename T>
class Gen<T>::IGenImpl {
public:
  virtual Shrinkable<T> generate(const Random &random, int size) const = 0;
  virtual void retain() = 0;
  virtual void release() = 0;
  virtual ~IGenImpl() = default;
};

template <typename T>
template <typename Impl>
class Gen<T>::GenImpl : public IGenImpl {
public:
  template <typename... Args>
  GenImpl(Args &&... args)
      : m_impl(std::forward<Args>(args)...)
      , m_count(1) {}

  Shrinkable<T> generate(const Random &random, int size) const override {
    return m_impl(random, size);
  }

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
template <typename Impl, typename>
Gen<T>::Gen(Impl &&impl)
    : m_impl(new GenImpl<Decay<Impl>>(std::forward<Impl>(impl))) {}

template <typename T>
std::string Gen<T>::name() const {
  return m_name;
}

template <typename T>
Shrinkable<T> Gen<T>::operator()(const Random &random, int size) const
    noexcept {
  try {
    return m_impl->generate(random, size);
  } catch (...) {
    auto exception = std::current_exception();
    return shrinkable::lambda([=]() -> T {
      std::rethrow_exception(exception);

      // MSVC HACK: the following is required for MSVC to stop complaining
      //            about missing return value. Will never be reached, of
      //            course.
#ifdef _MSC_VER
      throw nullptr;
#endif // _MSC_VER
    });
  }
}

template <typename T>
T Gen<T>::operator*() const {
  using namespace detail;
  using rc::gen::detail::param::CurrentHandler;
  const auto handler = ImplicitParam<CurrentHandler>::value();
  return std::move(handler->onGenerate(gen::map(*this, &Any::of<T>).as(m_name))
                       .template get<T>());
}

template <typename T>
Gen<T> Gen<T>::as(const std::string &name) const {
  auto gen = *this;
  gen.m_name = name;
  return gen;
}

template <typename T>
Gen<T>::Gen(const Gen &other) noexcept : m_impl(other.m_impl),
                                         m_name(other.m_name) {
  m_impl->retain();
}

template <typename T>
Gen<T> &Gen<T>::operator=(const Gen &rhs) noexcept {
  rhs.m_impl->retain();
  if (m_impl) {
    m_impl->release();
  }
  m_impl = rhs.m_impl;
  m_name = rhs.m_name;
  return *this;
}

template <typename T>
Gen<T>::Gen(Gen &&other) noexcept : m_impl(other.m_impl),
                                    m_name(std::move(other.m_name)) {
  other.m_impl = nullptr;
}

template <typename T>
Gen<T> &Gen<T>::operator=(Gen &&rhs) noexcept {
  if (m_impl) {
    m_impl->release();
  }
  m_impl = rhs.m_impl;
  rhs.m_impl = nullptr;
  m_name = std::move(rhs.m_name);
  return *this;
}

template <typename T>
Gen<T>::~Gen() noexcept {
  if (m_impl) {
    m_impl->release();
  }
}

} // namespace rc
