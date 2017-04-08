#pragma once

namespace rc {
namespace gen {
namespace detail {

template <typename T>
class MaybeGen {
public:
  MaybeGen(Gen<T> gen)
      : m_gen(std::move(gen)) {}

  Shrinkable<Maybe<T>> operator()(const Random &random, int size) const {
    auto r = random;
    const auto x = r.split().next() % (size + 1);
    if (x == 0) {
      return shrinkable::lambda([]{ return Maybe<T>(); });
    }

    return prependNothing(shrinkable::map(
        m_gen(r, size), [](T &&x) -> Maybe<T> { return std::move(x); }));
  }

private:
  static Shrinkable<Maybe<T>> prependNothing(Shrinkable<Maybe<T>> &&s) {
    return shrinkable::mapShrinks(
        std::move(s),
        [](Seq<Shrinkable<Maybe<T>>> &&shrinks) {
          return seq::concat(
              seq::just(shrinkable::lambda([] { return Maybe<T>(); })),
              seq::map(std::move(shrinks), &prependNothing));
        });
  };

  Gen<T> m_gen;
};

template <typename T>
struct DefaultArbitrary<Maybe<T>> {
  static Gen<Maybe<T>> arbitrary() { return gen::maybe(gen::arbitrary<T>()); }
};

} // namespace detail

template <typename T>
Gen<Maybe<T>> maybe(Gen<T> gen) {
  return detail::MaybeGen<T>(std::move(gen));
}

} // namespace gen
} // namespace rc
