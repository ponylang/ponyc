#pragma once

namespace rc {
namespace seq {

template <typename T>
std::size_t length(Seq<T> seq) {
  std::size_t l = 0;
  while (seq.next()) {
    l++;
  }

  return l;
}

template <typename T, typename Callable>
void forEach(Seq<T> seq, Callable callable) {
  Maybe<T> value;
  while ((value = seq.next())) {
    callable(std::move(*value));
  }
}

template <typename T>
Maybe<T> last(Seq<T> seq) {
  Maybe<T> prev = seq.next();
  if (!prev) {
    return Nothing;
  }

  Maybe<T> value;
  while ((value = seq.next())) {
    prev = value;
  }

  return prev;
}

template <typename T>
bool contains(Seq<T> seq, const T &value) {
  Maybe<T> x;
  while ((x = seq.next())) {
    if (*x == value) {
      return true;
    }
  }

  return false;
}

template <typename T, typename Predicate>
bool all(Seq<T> seq, Predicate pred) {
  Maybe<T> x;
  while ((x = seq.next())) {
    if (!pred(*x)) {
      return false;
    }
  }

  return true;
}

template <typename T, typename Predicate>
bool any(Seq<T> seq, Predicate pred) {
  Maybe<T> x;
  while ((x = seq.next())) {
    if (pred(*x)) {
      return true;
    }
  }

  return false;
}

template <typename T>
Maybe<T> at(Seq<T> seq, std::size_t index) {
  for (std::size_t i = 0; i < index; i++) {
    auto x = seq.next();
    if (!x) {
      return Nothing;
    }
  }

  return seq.next();
}

} // namespace seq
} // namespace rc
