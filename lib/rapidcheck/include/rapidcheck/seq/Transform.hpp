#pragma once

#include "rapidcheck/detail/ApplyTuple.h"
#include "rapidcheck/seq/Create.h"

namespace rc {
namespace seq {
namespace detail {

template <typename T>
class DropSeq {
public:
  DropSeq(std::size_t n, Seq<T> seq)
      : m_drop(n)
      , m_seq(std::move(seq)) {}

  Maybe<T> operator()() {
    while (m_drop > 0) {
      if (!m_seq.next()) {
        m_seq = Seq<T>();
        m_drop = 0;
        return Nothing;
      }
      m_drop--;
    }

    return m_seq.next();
  }

private:
  std::size_t m_drop;
  Seq<T> m_seq;
};

template <typename T>
class TakeSeq {
public:
  TakeSeq(std::size_t n, Seq<T> seq)
      : m_take(n)
      , m_seq(std::move(seq)) {}

  Maybe<T> operator()() {
    if (m_take == 0) {
      return Nothing;
    }

    m_take--;
    return m_seq.next();
  }

private:
  std::size_t m_take;
  Seq<T> m_seq;
};

template <typename Predicate, typename T>
class DropWhileSeq {
public:
  template <typename PredArg>
  DropWhileSeq(Seq<T> seq, PredArg &&pred)
      : m_pred(std::forward<PredArg>(pred))
      , m_dropped(false)
      , m_seq(std::move(seq)) {}

  Maybe<T> operator()() {
    while (!m_dropped) {
      auto value = m_seq.next();

      if (!value) {
        m_dropped = true;
        m_seq = Seq<T>();
        return Nothing;
      }

      if (!m_pred(*value)) {
        m_dropped = true;
        return value;
      }
    }

    return m_seq.next();
  }

private:
  Predicate m_pred;
  bool m_dropped;
  Seq<T> m_seq;
};

template <typename Predicate, typename T>
class TakeWhileSeq {
public:
  template <typename PredArg>
  TakeWhileSeq(Seq<T> seq, PredArg &&pred)
      : m_pred(std::forward<PredArg>(pred))
      , m_seq(std::move(seq)) {}

  Maybe<T> operator()() {
    auto value = m_seq.next();
    if (!value) {
      return Nothing;
    }

    if (!m_pred(*value)) {
      m_seq = Seq<T>();
      return Nothing;
    }

    return value;
  }

private:
  Predicate m_pred;
  Seq<T> m_seq;
};

template <typename Mapper, typename T>
class MapSeq {
public:
  using U = Decay<typename std::result_of<Mapper(T)>::type>;

  template <typename MapperArg>
  MapSeq(Seq<T> seq, MapperArg &&mapper)
      : m_mapper(std::forward<MapperArg>(mapper))
      , m_seq(std::move(seq)) {}

  Maybe<U> operator()() {
    auto value = m_seq.next();
    if (!value) {
      m_seq = Seq<T>();
      return Nothing;
    }

    return m_mapper(std::move(*value));
  }

private:
  Mapper m_mapper;
  Seq<T> m_seq;
};

template <typename Zipper, typename... Ts>
class ZipWithSeq {
public:
  using U = Decay<typename std::result_of<Zipper(Ts...)>::type>;

  template <typename ZipperArg>
  ZipWithSeq(ZipperArg &&zipper, Seq<Ts>... seqs)
      : m_zipper(std::forward<ZipperArg>(zipper))
      , m_seqs(std::move(seqs)...) {}

  Maybe<U> operator()() {
    return rc::detail::applyTuple(
        m_seqs,
        [this](Seq<Ts> &... seqs) { return mapMaybes(seqs.next()...); });
  }

private:
  Maybe<U> mapMaybes(Maybe<Ts> &&... maybes) {
    if (!allTrue(maybes...)) {
      m_seqs = std::tuple<Seq<Ts>...>();
      return Nothing;
    }

    return m_zipper(std::move(*maybes)...);
  }

  static bool allTrue() { return true; }

  template <typename MaybeT, typename... MaybeTs>
  static bool allTrue(const MaybeT &arg, const MaybeTs &... args) {
    return arg && allTrue(args...);
  }

  Zipper m_zipper;
  std::tuple<Seq<Ts>...> m_seqs;
};

template <typename Predicate, typename T>
class FilterSeq {
public:
  template <typename PredArg>
  FilterSeq(Seq<T> seq, PredArg predicate)
      : m_predicate(std::forward<PredArg>(predicate))
      , m_seq(std::move(seq)) {}

  Maybe<T> operator()() {
    while (true) {
      auto value = m_seq.next();

      if (!value) {
        m_seq = Seq<T>();
        return Nothing;
      }

      if (m_predicate(*value)) {
        return value;
      }
    }
  }

private:
  Predicate m_predicate;
  Seq<T> m_seq;
};

template <typename T>
class JoinSeq {
public:
  JoinSeq(Seq<Seq<T>> seqs)
      : m_seqs(std::move(seqs)) {}

  Maybe<T> operator()() {
    while (true) {
      auto value = m_seq.next();
      if (value) {
        return value;
      }

      // Otherwise, next Seq
      auto seq = m_seqs.next();
      if (!seq) {
        m_seq = Seq<T>();
        m_seqs = Seq<Seq<T>>();
        return Nothing;
      }

      m_seq = std::move(*seq);
    }
  }

private:
  Seq<T> m_seq;
  Seq<Seq<T>> m_seqs;
};

template <typename T, std::size_t N>
class ConcatSeq {
public:
  template <typename... Ts>
  ConcatSeq(Seq<Ts>... seqs)
      : m_seqs{std::move(seqs)...}
      , m_i(0) {}

  Maybe<T> operator()() {
    while (m_i < N) {
      auto value = m_seqs[m_i].next();
      if (value) {
        return value;
      }

      m_i++;
    }

    return Nothing;
  }

private:
  Seq<T> m_seqs[N];
  std::size_t m_i;
};

template <typename Mapper, typename T>
class MapcatSeq {
public:
  using U = typename std::result_of<Mapper(T)>::type::ValueType;

  template <typename MapperArg>
  MapcatSeq(Seq<T> seq, MapperArg &&mapper)
      : m_mapper(std::forward<MapperArg>(mapper))
      , m_seqT(std::move(seq)) {}

  Maybe<U> operator()() {
    while (true) {
      auto valueU = m_seqU.next();
      if (valueU) {
        return valueU;
      }

      auto valueT = m_seqT.next();
      if (!valueT) {
        m_seqT = Seq<T>();
        m_seqU = Seq<U>();
        return Nothing;
      }

      m_seqU = m_mapper(std::move(*valueT));
    }
  }

private:
  Mapper m_mapper;
  Seq<T> m_seqT;
  Seq<U> m_seqU;
};

} // namespace detail

template <typename T>
Seq<T> drop(std::size_t n, Seq<T> seq) {
  if (n == 0) {
    return seq;
  }
  return makeSeq<detail::DropSeq<T>>(n, std::move(seq));
}

template <typename T>
Seq<T> take(std::size_t n, Seq<T> seq) {
  if (n == 0) {
    return Seq<T>();
  }
  return makeSeq<detail::TakeSeq<T>>(n, std::move(seq));
}

template <typename T, typename Predicate>
Seq<T> dropWhile(Seq<T> seq, Predicate &&pred) {
  return makeSeq<detail::DropWhileSeq<Decay<Predicate>, T>>(
      std::move(seq), std::forward<Predicate>(pred));
}

template <typename T, typename Predicate>
Seq<T> takeWhile(Seq<T> seq, Predicate &&pred) {
  return makeSeq<detail::TakeWhileSeq<Decay<Predicate>, T>>(
      std::move(seq), std::forward<Predicate>(pred));
}

template <typename T, typename Mapper>
Seq<Decay<typename std::result_of<Mapper(T)>::type>> map(Seq<T> seq,
                                                         Mapper &&mapper) {
  return makeSeq<detail::MapSeq<Decay<Mapper>, T>>(
      std::move(seq), std::forward<Mapper>(mapper));
}

template <typename... Ts, typename Zipper>
Seq<Decay<typename std::result_of<Zipper(Ts...)>::type>>
zipWith(Zipper &&zipper, Seq<Ts>... seqs) {
  return makeSeq<detail::ZipWithSeq<Decay<Zipper>, Ts...>>(
      std::forward<Zipper>(zipper), std::move(seqs)...);
}

template <typename T, typename Predicate>
Seq<T> filter(Seq<T> seq, Predicate &&pred) {
  return makeSeq<detail::FilterSeq<Decay<Predicate>, T>>(
      std::move(seq), std::forward<Predicate>(pred));
}

template <typename T>
Seq<T> join(Seq<Seq<T>> seqs) {
  return makeSeq<detail::JoinSeq<T>>(std::move(seqs));
}

template <typename T, typename... Ts>
Seq<T> concat(Seq<T> seq, Seq<Ts>... seqs) {
  return makeSeq<detail::ConcatSeq<T, sizeof...(Ts) + 1>>(std::move(seq),
                                                          std::move(seqs)...);
}

template <typename T, typename Mapper>
Seq<typename std::result_of<Mapper(T)>::type::ValueType>
mapcat(Seq<T> seq, Mapper &&mapper) {
  return makeSeq<detail::MapcatSeq<Decay<Mapper>, T>>(
      std::move(seq), std::forward<Mapper>(mapper));
}

template <typename T, typename Mapper>
Seq<typename std::result_of<Mapper(T)>::type::ValueType>
mapMaybe(Seq<T> seq, Mapper &&mapper) {
  using U = typename std::result_of<Mapper(T)>::type::ValueType;
  return seq::map(
      seq::filter(seq::map(std::move(seq), std::forward<Mapper>(mapper)),
                  [](const Maybe<U> &x) { return !!x; }),
      [](Maybe<U> &&x) { return std::move(*x); });
}

template <typename T>
Seq<T> cycle(Seq<T> seq) {
  return seq::join(seq::repeat(std::move(seq)));
}

template <typename T, typename U>
Seq<T> cast(Seq<U> seq) {
  return seq::map(std::move(seq),
                  [](U &&x) { return static_cast<T>(std::move(x)); });
}

} // namespace seq
} // namespace rc
