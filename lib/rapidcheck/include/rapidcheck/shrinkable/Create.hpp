#pragma once

#include "rapidcheck/fn/Common.h"
#include "rapidcheck/seq/Transform.h"

namespace rc {
namespace shrinkable {
namespace detail {

template <typename Value, typename Shrink>
class LambdaShrinkable {
public:
  using T = Decay<typename std::result_of<Value()>::type>;

  template <typename ValueArg, typename ShrinkArg>
  LambdaShrinkable(ValueArg &&value, ShrinkArg &&shrinks)
      : m_value(std::forward<ValueArg>(value))
      , m_shrinks(std::forward<ShrinkArg>(shrinks)) {}

  T value() const { return m_value(); }
  Seq<Shrinkable<T>> shrinks() const { return m_shrinks(); }

private:
  Value m_value;
  Shrink m_shrinks;
};

template <typename Value, typename Shrink>
class JustShrinkShrinkable // Yeah I know, weird name
    {
public:
  using T = Decay<typename std::result_of<Value()>::type>;

  template <typename ValueArg, typename ShrinkArg>
  JustShrinkShrinkable(ValueArg &&value, ShrinkArg &&shrinks)
      : m_value(std::forward<ValueArg>(value))
      , m_shrink(std::forward<ShrinkArg>(shrinks)) {}

  T value() const { return m_value(); }
  Seq<Shrinkable<T>> shrinks() const { return m_shrink(m_value()); }

private:
  Value m_value;
  Shrink m_shrink;
};

} // namespace detail

// TODO test _all_ of these?

template <typename Value, typename Shrink>
Shrinkable<typename std::result_of<Value()>::type> lambda(Value &&value,
                                                          Shrink &&shrinks) {
  using Impl = detail::LambdaShrinkable<Decay<Value>, Decay<Shrink>>;
  return makeShrinkable<Impl>(std::forward<Value>(value),
                              std::forward<Shrink>(shrinks));
}

template <typename Value>
Shrinkable<typename std::result_of<Value()>::type> lambda(Value &&value) {
  using T = typename std::result_of<Value()>::type;
  return shrinkable::lambda(std::forward<Value>(value),
                            fn::constant(Seq<Shrinkable<T>>()));
}

template <typename T, typename Value, typename>
Shrinkable<T> just(Value &&value, Seq<Shrinkable<T>> shrinks) {
  return shrinkable::lambda(fn::constant(std::forward<Value>(value)),
                            fn::constant(std::move(shrinks)));
}

template <typename T>
Shrinkable<Decay<T>> just(T &&value) {
  return shrinkable::just(std::forward<T>(value), Seq<Shrinkable<Decay<T>>>());
}

template <typename Value, typename Shrink>
Shrinkable<typename std::result_of<Value()>::type> shrink(Value &&value,
                                                          Shrink &&shrinkf) {
  using Impl = detail::JustShrinkShrinkable<Decay<Value>, Decay<Shrink>>;
  return makeShrinkable<Impl>(std::forward<Value>(value),
                              std::forward<Shrink>(shrinkf));
}

template <typename T, typename Shrink>
Shrinkable<Decay<T>> shrinkRecur(T &&value, const Shrink &shrinkf) {
  return shrinkable::shrink(fn::constant(std::forward<T>(value)),
                            [=](Decay<T> &&x) {
                              return seq::map(shrinkf(std::move(x)),
                                              [=](Decay<T> &&xshrink) {
                                                return shrinkable::shrinkRecur(
                                                    std::move(xshrink),
                                                    shrinkf);
                                              });
                            });
}

} // namespace shrinkable
} // namespace rc
