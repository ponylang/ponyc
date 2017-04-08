#pragma once

#include <map>
#include <vector>
#include <deque>
#include <forward_list>
#include <list>
#include <set>
#include <unordered_set>
#include <unordered_map>
#include <memory>
#include <cstdint>
#include <type_traits>
#include <array>
#include <sstream>

#include "rapidcheck/detail/Traits.h"

namespace rc {
namespace detail {

template <typename TupleT, std::size_t I = std::tuple_size<TupleT>::value>
struct TupleHelper;

template <std::size_t I>
struct TupleHelper<std::tuple<>, I> {
  static void showTuple(const std::tuple<> &tuple, std::ostream &os) {}
};

template <typename TupleT>
struct TupleHelper<TupleT, 1> {
  static void showTuple(const TupleT &tuple, std::ostream &os) {
    show(std::get<std::tuple_size<TupleT>::value - 1>(tuple), os);
  }
};

template <typename TupleT, std::size_t I>
struct TupleHelper {
  static void showTuple(const TupleT &tuple, std::ostream &os) {
    show(std::get<std::tuple_size<TupleT>::value - I>(tuple), os);
    os << ", ";
    TupleHelper<TupleT, I - 1>::showTuple(tuple, os);
  }
};

template <typename T>
void showValue(T value,
               typename std::enable_if<std::is_same<T, bool>::value,
                                       std::ostream>::type &os) {
  os << (value ? "true" : "false");
}

template <typename T>
void showValue(T value,
               typename std::enable_if<(std::is_same<T, char>::value ||
                                        std::is_same<T, unsigned char>::value),
                                       std::ostream>::type &os) {
  os << static_cast<int>(value);
}

void showValue(const std::string &value, std::ostream &os);
void showValue(const char *value, std::ostream &os);

template <typename T>
void showValue(T *p, std::ostream &os) {
  show(*p, os);
  auto flags = os.flags();
  os << " (" << std::hex << std::showbase << p << ")";
  os.flags(flags);
}

template <typename T, typename Deleter>
void showValue(const std::unique_ptr<T, Deleter> &p, std::ostream &os) {
  show(p.get(), os);
}

template <typename T>
void showValue(const std::shared_ptr<T> &p, std::ostream &os) {
  show(p.get(), os);
}

template <typename T1, typename T2>
void showValue(const std::pair<T1, T2> &pair, std::ostream &os) {
  os << "(";
  show(pair.first, os);
  os << ", ";
  show(pair.second, os);
  os << ")";
}

template <typename... Types>
void showValue(const std::tuple<Types...> &tuple, std::ostream &os) {
  os << "(";
  detail::TupleHelper<std::tuple<Types...>>::showTuple(tuple, os);
  os << ")";
}

template <typename T, typename Allocator>
void showValue(const std::vector<T, Allocator> &value, std::ostream &os) {
  showCollection("[", "]", value, os);
}

template <typename T, typename Allocator>
void showValue(const std::deque<T, Allocator> &value, std::ostream &os) {
  showCollection("[", "]", value, os);
}

template <typename T, typename Allocator>
void showValue(const std::forward_list<T, Allocator> &value, std::ostream &os) {
  showCollection("[", "]", value, os);
}

template <typename T, typename Allocator>
void showValue(const std::list<T, Allocator> &value, std::ostream &os) {
  showCollection("[", "]", value, os);
}

template <typename Key, typename Compare, typename Allocator>
void showValue(const std::set<Key, Compare, Allocator> &value,
               std::ostream &os) {
  showCollection("{", "}", value, os);
}

template <typename Key, typename T, typename Compare, typename Allocator>
void showValue(const std::map<Key, T, Compare, Allocator> &value,
               std::ostream &os) {
  showCollection("{", "}", value, os);
}

template <typename Key, typename Compare, typename Allocator>
void showValue(const std::multiset<Key, Compare, Allocator> &value,
               std::ostream &os) {
  showCollection("{", "}", value, os);
}

template <typename Key, typename T, typename Compare, typename Allocator>
void showValue(const std::multimap<Key, T, Compare, Allocator> &value,
               std::ostream &os) {
  showCollection("{", "}", value, os);
}

template <typename Key, typename Hash, typename KeyEqual, typename Allocator>
void showValue(const std::unordered_set<Key, Hash, KeyEqual, Allocator> &value,
               std::ostream &os) {
  showCollection("{", "}", value, os);
}

template <typename Key,
          typename T,
          typename Hash,
          typename KeyEqual,
          typename Allocator>
void showValue(
    const std::unordered_map<Key, T, Hash, KeyEqual, Allocator> &value,
    std::ostream &os) {
  showCollection("{", "}", value, os);
}

template <typename Key, typename Hash, typename KeyEqual, typename Allocator>
void showValue(
    const std::unordered_multiset<Key, Hash, KeyEqual, Allocator> &value,
    std::ostream &os) {
  showCollection("{", "}", value, os);
}

template <typename Key,
          typename T,
          typename Hash,
          typename KeyEqual,
          typename Allocator>
void showValue(
    const std::unordered_multimap<Key, T, Hash, KeyEqual, Allocator> &value,
    std::ostream &os) {
  showCollection("{", "}", value, os);
}

template <typename CharT, typename Traits, typename Allocator>
void showValue(const std::basic_string<CharT, Traits, Allocator> &value,
               std::ostream &os) {
  showCollection("\"", "\"", value, os);
}

template <typename T, std::size_t N>
void showValue(const std::array<T, N> &value, std::ostream &os) {
  showCollection("[", "]", value, os);
}

RC_SFINAE_TRAIT(HasShowValue, decltype(showValue(std::declval<T>(), std::cout)))

template <typename T,
          bool = HasShowValue<T>::value,
          bool = IsStreamInsertible<T>::value>
struct ShowDefault {
  static void show(const T &/*value*/, std::ostream &os) { os << "<\?\?\?>"; }
};

template <typename T, bool X>
struct ShowDefault<T, true, X> {
  static void show(const T &value, std::ostream &os) { showValue(value, os); }
};

template <typename T>
struct ShowDefault<T, false, true> {
  static void show(const T &value, std::ostream &os) { os << value; }
};

} // namespace detail

template <typename T>
void show(const T &value, std::ostream &os) {
  detail::ShowDefault<T>::show(value, os);
}

template <typename T>
std::string toString(const T &value) {
  std::ostringstream os;
  show(value, os);
  return os.str();
}

template <typename Collection>
void showCollection(const std::string &prefix,
                    const std::string &suffix,
                    const Collection &collection,
                    std::ostream &os) {
  os << prefix;
  auto cbegin = begin(collection);
  auto cend = end(collection);
  if (cbegin != cend) {
    show(*cbegin, os);
    for (auto it = ++cbegin; it != cend; it++) {
      os << ", ";
      show(*it, os);
    }
  }
  os << suffix;
}

} // namespace rc
