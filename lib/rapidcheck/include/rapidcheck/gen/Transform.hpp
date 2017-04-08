#pragma once

#include "rapidcheck/shrinkable/Transform.h"
#include "rapidcheck/gen/Arbitrary.h"
#include "rapidcheck/gen/Tuple.h"
#include "rapidcheck/GenerationFailure.h"
#include "rapidcheck/Random.h"

namespace rc {
namespace gen {
namespace detail {

template <typename T, typename Mapper>
class MapGen {
public:
  using U = Decay<typename std::result_of<Mapper(T)>::type>;

  template <typename MapperArg>
  MapGen(Gen<T> gen, MapperArg &&mapper)
      : m_mapper(std::forward<MapperArg>(mapper))
      , m_gen(std::move(gen)) {}

  Shrinkable<U> operator()(const Random &random, int size) const {
    return shrinkable::map(m_gen(random, size), m_mapper);
  }

private:
  Mapper m_mapper;
  Gen<T> m_gen;
};

template <typename T, typename Mapper>
class MapcatGen {
public:
  using U = typename std::result_of<Mapper(T)>::type::ValueType;

  template <typename MapperArg>
  explicit MapcatGen(Gen<T> gen, MapperArg &&mapper)
      : m_gen(std::move(gen))
      , m_mapper(std::forward<MapperArg>(mapper)) {}

  Shrinkable<U> operator()(const Random &random, int size) const {
    auto r1 = random;
    auto r2 = r1.split();
    const auto mapper = m_mapper;
    return shrinkable::mapcat(
        m_gen(r1, size), [=](T &&x) { return mapper(std::move(x))(r2, size); });
  }

private:
  Gen<T> m_gen;
  Mapper m_mapper;
};

template <typename T>
class JoinGen {
public:
  explicit JoinGen(Gen<Gen<T>> gen)
      : m_gen(std::move(gen)) {}

  Shrinkable<T> operator()(const Random &random, int size) const {
    auto r1 = random;
    auto r2 = r1.split();
    return shrinkable::mapcat(
        m_gen(r1, size),
        [=](const Gen<T> &innerGen) { return innerGen(r2, size); });
  }

private:
  Gen<Gen<T>> m_gen;
};

} // namespace detail

template <typename T, typename Mapper>
Gen<Decay<typename std::result_of<Mapper(T)>::type>> map(Gen<T> gen,
                                                         Mapper &&mapper) {
  return detail::MapGen<T, Decay<Mapper>>(std::move(gen),
                                          std::forward<Mapper>(mapper));
}

template <typename T, typename Mapper>
Gen<Decay<typename std::result_of<Mapper(T)>::type>> map(Mapper &&mapper) {
  return gen::map(gen::arbitrary<T>(), std::forward<Mapper>(mapper));
}

template <typename T, typename Mapper>
Gen<typename std::result_of<Mapper(T)>::type::ValueType>
mapcat(Gen<T> gen, Mapper &&mapper) {
  return detail::MapcatGen<T, Decay<Mapper>>(std::move(gen),
                                             std::forward<Mapper>(mapper));
}

template <typename T>
Gen<T> join(Gen<Gen<T>> gen) {
  return detail::JoinGen<T>(std::move(gen));
}

template <typename Callable, typename... Ts>
Gen<typename std::result_of<Callable(Ts...)>::type> apply(Callable &&callable,
                                                          Gen<Ts>... gens) {
  return gen::map(gen::tuple(std::move(gens)...),
                  [=](std::tuple<Ts...> &&tuple) {
                    return rc::detail::applyTuple(std::move(tuple), callable);
                  });
}

template <typename T, typename U>
Gen<T> cast(Gen<U> gen) {
  return gen::map(std::move(gen),
                  [](U &&x) { return static_cast<T>(std::move(x)); });
}

template <typename T>
Gen<T> resize(int size, Gen<T> gen) {
  return [=](const Random &random, int) { return gen(random, size); };
}

template <typename T>
Gen<T> scale(double scale, Gen<T> gen) {
  return [=](const Random &random, int size) {
    return gen(random, static_cast<int>(size * scale));
  };
}

template <typename Callable>
Gen<typename std::result_of<Callable(int)>::type::ValueType>
withSize(Callable &&callable) {
  return [=](const Random &random, int size) {
    return callable(size)(random, size);
  };
}

template <typename T>
Gen<T> noShrink(Gen<T> gen) {
  return [=](const Random &random, int size) {
    return shrinkable::mapShrinks(gen(random, size),
                                  fn::constant(Seq<Shrinkable<T>>()));
  };
}

template <typename T, typename Shrink>
Gen<T> shrink(Gen<T> gen, Shrink &&shrink) {
  return [=](const Random &random, int size) {
    return shrinkable::postShrink(gen(random, size), shrink);
  };
}

} // namespace gen
} // namespace rc
