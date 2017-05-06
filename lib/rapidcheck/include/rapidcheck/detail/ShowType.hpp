#pragma once

#include <vector>
#include <deque>
#include <forward_list>
#include <list>
#include <set>
#include <map>
#include <unordered_set>
#include <unordered_map>
#include <stack>
#include <sstream>

#include "rapidcheck/detail/Platform.h"
#include "rapidcheck/detail/Traits.h"

namespace rc {
namespace detail {

template <typename... Types>
struct ShowMultipleTypes;

template <>
struct ShowMultipleTypes<> {
  static void showType(std::ostream &/*os*/) {}
};

template <typename Type>
struct ShowMultipleTypes<Type> {
  static void showType(std::ostream &os) { detail::showType<Type>(os); }
};

template <typename Type, typename... Types>
struct ShowMultipleTypes<Type, Types...> {
  static void showType(std::ostream &os) {
    detail::showType<Type>(os);
    os << ", ";
    ShowMultipleTypes<Types...>::showType(os);
  }
};

} // namespace detail

template <typename T>
struct ShowType {
  static void showType(std::ostream &os) {
    os << detail::demangle(typeid(T).name());
  }
};

template <typename T>
struct ShowType<const T> {
  static void showType(std::ostream &os) {
    os << "const ";
    detail::showType<T>(os);
  }
};

template <typename T>
struct ShowType<volatile T> {
  static void showType(std::ostream &os) {
    os << "volatile ";
    detail::showType<T>(os);
  }
};

template <typename T>
struct ShowType<const volatile T> {
  static void showType(std::ostream &os) {
    os << "const volatile ";
    detail::showType<T>(os);
  }
};

template <typename T>
struct ShowType<T &> {
  static void showType(std::ostream &os) {
    detail::showType<T>(os);
    os << " &";
  }
};

template <typename T>
struct ShowType<T &&> {
  static void showType(std::ostream &os) {
    detail::showType<T>(os);
    os << " &&";
  }
};

template <typename T>
struct ShowType<T *> {
  static void showType(std::ostream &os) {
    detail::showType<T>(os);
    os << " *";
  }
};

// To avoid whitespace between asterisks
template <typename T>
struct ShowType<T **> {
  static void showType(std::ostream &os) {
    detail::showType<T *>(os);
    os << "*";
  }
};

template <typename Traits, typename Allocator>
struct ShowType<std::basic_string<char, Traits, Allocator>> {
  static void showType(std::ostream &os) { os << "std::string"; }
};

template <typename Traits, typename Allocator>
struct ShowType<std::basic_string<wchar_t, Traits, Allocator>> {
  static void showType(std::ostream &os) { os << "std::wstring"; }
};

template <typename Traits, typename Allocator>
struct ShowType<std::basic_string<char16_t, Traits, Allocator>> {
  static void showType(std::ostream &os) { os << "std::u16string"; }
};

template <typename Traits, typename Allocator>
struct ShowType<std::basic_string<char32_t, Traits, Allocator>> {
  static void showType(std::ostream &os) { os << "std::u32string"; }
};

template <typename T, typename Allocator>
struct ShowType<std::vector<T, Allocator>> {
  static void showType(std::ostream &os) {
    os << "std::vector<";
    detail::showType<T>(os);
    os << ">";
  }
};

template <typename T, typename Allocator>
struct ShowType<std::deque<T, Allocator>> {
  static void showType(std::ostream &os) {
    os << "std::deque<";
    detail::showType<T>(os);
    os << ">";
  }
};

template <typename T, typename Allocator>
struct ShowType<std::forward_list<T, Allocator>> {
  static void showType(std::ostream &os) {
    os << "std::forward_list<";
    detail::showType<T>(os);
    os << ">";
  }
};

template <typename T, typename Allocator>
struct ShowType<std::list<T, Allocator>> {
  static void showType(std::ostream &os) {
    os << "std::list<";
    detail::showType<T>(os);
    os << ">";
  }
};

template <typename Key, typename Compare, typename Allocator>
struct ShowType<std::set<Key, Compare, Allocator>> {
  static void showType(std::ostream &os) {
    os << "std::set<";
    detail::showType<Key>(os);
    os << ">";
  }
};

template <typename Key, typename T, typename Compare, typename Allocator>
struct ShowType<std::map<Key, T, Compare, Allocator>> {
  static void showType(std::ostream &os) {
    os << "std::map<";
    detail::showType<Key>(os);
    os << ", ";
    detail::showType<T>(os);
    os << ">";
  }
};

template <typename Key, typename Compare, typename Allocator>
struct ShowType<std::multiset<Key, Compare, Allocator>> {
  static void showType(std::ostream &os) {
    os << "std::multiset<";
    detail::showType<Key>(os);
    os << ">";
  }
};

template <typename Key, typename T, typename Compare, typename Allocator>
struct ShowType<std::multimap<Key, T, Compare, Allocator>> {
  static void showType(std::ostream &os) {
    os << "std::multimap<";
    detail::showType<Key>(os);
    os << ", ";
    detail::showType<T>(os);
    os << ">";
  }
};

template <typename Key, typename Hash, typename KeyEqual, typename Allocator>
struct ShowType<std::unordered_set<Key, Hash, KeyEqual, Allocator>> {
  static void showType(std::ostream &os) {
    os << "std::unordered_set<";
    detail::showType<Key>(os);
    os << ">";
  }
};

template <typename Key,
          typename T,
          typename Hash,
          typename KeyEqual,
          typename Allocator>
struct ShowType<std::unordered_map<Key, T, Hash, KeyEqual, Allocator>> {
  static void showType(std::ostream &os) {
    os << "std::unordered_map<";
    detail::showType<Key>(os);
    os << ", ";
    detail::showType<T>(os);
    os << ">";
  }
};

template <typename Key,
          typename T,
          typename Hash,
          typename KeyEqual,
          typename Allocator>
struct ShowType<std::unordered_multimap<Key, T, Hash, KeyEqual, Allocator>> {
  static void showType(std::ostream &os) {
    os << "std::unordered_multimap<";
    detail::showType<Key>(os);
    os << ", ";
    detail::showType<T>(os);
    os << ">";
  }
};

template <typename Key, typename Hash, typename KeyEqual, typename Allocator>
struct ShowType<std::unordered_multiset<Key, Hash, KeyEqual, Allocator>> {
  static void showType(std::ostream &os) {
    os << "std::unordered_multiset<";
    detail::showType<Key>(os);
    os << ">";
  }
};

template <typename T, std::size_t N>
struct ShowType<std::array<T, N>> {
  static void showType(std::ostream &os) {
    os << "std::array<";
    detail::showType<T>(os);
    os << ", " << N << ">";
  }
};

template <typename T, typename Container>
struct ShowType<std::stack<T, Container>> {
  static void showType(std::ostream &os) {
    os << "std::stack<";
    detail::showType<T>(os);
    os << ">";
  }
};

template <typename T1, typename T2>
struct ShowType<std::pair<T1, T2>> {
  static void showType(std::ostream &os) {
    os << "std::pair<";
    detail::showType<T1>(os);
    os << ", ";
    detail::showType<T2>(os);
    os << ">";
  }
};

template <typename... Types>
struct ShowType<std::tuple<Types...>> {
  static void showType(std::ostream &os) {
    os << "std::tuple<";
    detail::ShowMultipleTypes<Types...>::showType(os);
    os << ">";
  }
};

template <typename T, typename Deleter>
struct ShowType<std::unique_ptr<T, Deleter>> {
  static void showType(std::ostream &os) {
    os << "std::unique_ptr<";
    detail::showType<T>(os);
    os << ">";
  }
};

template <typename T>
struct ShowType<std::shared_ptr<T>> {
  static void showType(std::ostream &os) {
    os << "std::shared_ptr<";
    detail::showType<T>(os);
    os << ">";
  }
};

namespace detail {

template <typename T>
void showType(std::ostream &os) {
  rc::ShowType<T>::showType(os);
}

template <typename T>
std::string typeToString() {
  std::ostringstream ss;
  showType<T>(ss);
  return ss.str();
}

} // namespace detail
} // namespace rc
