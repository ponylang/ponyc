#pragma once

namespace rc {
namespace detail {

template <typename... Types>
struct IndexHelper;

template <>
struct IndexHelper<> {
  template <typename T>
  static constexpr std::size_t indexOf() {
    return 0;
  }
};

template <typename First, typename... Types>
struct IndexHelper<First, Types...> {
  template <typename T>
  static constexpr std::size_t indexOf() {
    return std::is_same<First, T>::value
        ? 0
        : IndexHelper<Types...>::template indexOf<T>() + 1;
  }
};

template <typename Type, typename... Types>
template <typename T, typename>
Variant<Type, Types...>::Variant(T &&value) noexcept(
    std::is_nothrow_constructible<Decay<T>, T &&>::value)
    : m_typeIndex(indexOfType<Decay<T>>()) {
  static_assert(isValidType<Decay<T>>(),
                "T is not a valid type of this variant");

  new (&m_storage) Decay<T>(std::forward<T>(value));
}

template <typename Type, typename... Types>
template <typename T, typename>
Variant<Type, Types...> &Variant<Type, Types...>::
operator=(const T &value) noexcept {
  static_assert(isValidType<T>(), "T is not a valid type of this variant");

  const auto newIndex = indexOfType<T>();
  if (newIndex == m_typeIndex) {
    *reinterpret_cast<T *>(&m_storage) = value;
  } else {
    destroy(m_typeIndex, &m_storage);
    m_typeIndex = newIndex;
    new (&m_storage) T(value);
  }
  return *this;
}

template <typename Type, typename... Types>
template <typename T, typename>
Variant<Type, Types...> &Variant<Type, Types...>::
operator=(T &&value) noexcept {
  static_assert(isValidType<T>(), "T is not a valid type of this variant");

  const auto newIndex = indexOfType<T>();
  if (newIndex == m_typeIndex) {
    *reinterpret_cast<T *>(&m_storage) = std::move(value);
  } else {
    destroy(m_typeIndex, &m_storage);
    m_typeIndex = newIndex;
    new (&m_storage) T(std::move(value));
  }
  return *this;
}

template <typename Type, typename... Types>
template <typename T>
T &Variant<Type, Types...>::get() & {
  assert(indexOfType<T>() == m_typeIndex);
  return *reinterpret_cast<T *>(&m_storage);
}

template <typename Type, typename... Types>
template <typename T>
const T &Variant<Type, Types...>::get() const & {
  assert(indexOfType<T>() == m_typeIndex);
  return *reinterpret_cast<const T *>(&m_storage);
}

template <typename Type, typename... Types>
template <typename T>
T &&Variant<Type, Types...>::get() && {
  return std::move(get<T>());
}

// TODO this would be more fun with varargs and lambdas
template <typename Type, typename... Types>
template <typename T>
bool Variant<Type, Types...>::match(T &value) const {
  if (!is<T>()) {
    return false;
  }

  value = *reinterpret_cast<const T *>(&m_storage);
  return true;
}

template <typename Type, typename... Types>
template <typename T>
bool Variant<Type, Types...>::is() const {
  static_assert(isValidType<Decay<T>>(),
                "T is not a valid type of this variant");
  return m_typeIndex == indexOfType<T>();
}

template <typename T>
bool variantEqualsImpl(const void *lhs, const void *rhs) {
  return *static_cast<const T *>(lhs) == *static_cast<const T *>(rhs);
}

template <typename Type, typename... Types>
bool Variant<Type, Types...>::operator==(const Variant &rhs) const {
  if (m_typeIndex != rhs.m_typeIndex) {
    return false;
  }

  static bool (*const equalsFuncs[])(const void *, const void *) = {
      &variantEqualsImpl<Type>, &variantEqualsImpl<Types>...};

  return equalsFuncs[m_typeIndex](&m_storage, &rhs.m_storage);
}

template <typename T>
void variantPrintToImpl(std::ostream &os, const void *storage) {
  os << *static_cast<const T *>(storage);
}

template <typename Type, typename... Types>
void Variant<Type, Types...>::printTo(std::ostream &os) const {
  static void (*printToFuncs[])(std::ostream &, const void *) = {
      &variantPrintToImpl<Type>, &variantPrintToImpl<Types>...};

  printToFuncs[m_typeIndex](os, &m_storage);
}

template <typename Type, typename... Types>
Variant<Type, Types...>::Variant(const Variant &other) noexcept(
    AllIs<std::is_nothrow_copy_constructible, Type, Types...>::value)
    : m_typeIndex(other.m_typeIndex) {
  copy(m_typeIndex, &m_storage, &other.m_storage);
}

template <typename Type, typename... Types>
Variant<Type, Types...>::Variant(Variant &&other) noexcept(
    AllIs<std::is_nothrow_move_constructible, Type, Types...>::value)
    : m_typeIndex(other.m_typeIndex) {
  move(m_typeIndex, &m_storage, &other.m_storage);
}

template <typename Type, typename... Types>
Variant<Type, Types...> &Variant<Type, Types...>::
operator=(const Variant &rhs) noexcept(
    AllIs<std::is_nothrow_copy_constructible, Type, Types...>::value
        &&AllIs<std::is_nothrow_copy_assignable, Type, Types...>::value) {
  static_assert(
      AllIs<std::is_nothrow_move_constructible, Type, Types...>::value,
      "AllIs types must be nothrow move-constructible to use copy assignment");

  if (m_typeIndex == rhs.m_typeIndex) {
    copyAssign(m_typeIndex, &m_storage, &rhs.m_storage);
  } else {
    Storage tmp;
    copy(rhs.m_typeIndex, &tmp, &rhs.m_storage);
    destroy(m_typeIndex, &m_storage);
    move(rhs.m_typeIndex, &m_storage, &tmp);
    m_typeIndex = rhs.m_typeIndex;
  }

  return *this;
}

template <typename Type, typename... Types>
Variant<Type, Types...> &Variant<Type, Types...>::
operator=(Variant &&rhs) noexcept(
    AllIs<std::is_nothrow_move_assignable, Type, Types...>::value) {
  static_assert(
      AllIs<std::is_nothrow_move_constructible, Type, Types...>::value,
      "AllIs types must be nothrow move-constructible to use copy assignment");

  if (m_typeIndex == rhs.m_typeIndex) {
    moveAssign(m_typeIndex, &m_storage, &rhs.m_storage);
  } else {
    destroy(m_typeIndex, &m_storage);
    m_typeIndex = rhs.m_typeIndex;
    move(m_typeIndex, &m_storage, &rhs.m_storage);
  }

  return *this;
}

template <typename Type, typename... Types>
Variant<Type, Types...>::~Variant() noexcept {
  destroy(m_typeIndex, &m_storage);
}

template <typename T>
void variantCopyAssign(void *to, const void *from) {
  *static_cast<T *>(to) = *static_cast<const T *>(from);
}

template <typename Type, typename... Types>
void Variant<Type, Types...>::copyAssign(std::size_t index,
                                         void *to,
                                         const void *from) {
  static void (*const copyAssignFuncs[])(void *, const void *) = {
      &variantCopyAssign<Type>, &variantCopyAssign<Types>...};

  copyAssignFuncs[index](to, from);
}

template <typename T>
void variantMoveAssign(void *to, void *from) {
  *static_cast<T *>(to) = std::move(*static_cast<T *>(from));
}

template <typename Type, typename... Types>
void Variant<Type, Types...>::moveAssign(std::size_t index,
                                         void *to,
                                         void *from) {
  static void (*const moveAssignFuncs[])(void *, void *) = {
      &variantMoveAssign<Type>, &variantMoveAssign<Types>...};

  moveAssignFuncs[index](to, from);
}

template <typename T>
void variantCopy(void *to, const void *from) {
  new (to) T(*static_cast<const T *>(from));
}

template <typename Type, typename... Types>
void Variant<Type, Types...>::copy(std::size_t index,
                                   void *to,
                                   const void *from) {
  static void (*const copyFuncs[])(void *, const void *) = {
      &variantCopy<Type>, &variantCopy<Types>...};

  copyFuncs[index](to, from);
}

template <typename T>
void variantMove(void *to, void *from) {
  new (to) T(std::move(*static_cast<T *>(from)));
}

template <typename Type, typename... Types>
void Variant<Type, Types...>::move(std::size_t index, void *to, void *from) {
  static void (*const moveFuncs[])(void *, void *) = {&variantMove<Type>,
                                                      &variantMove<Types>...};

  moveFuncs[index](to, from);
}

template <typename T>
void variantDestroy(void *storage) {
  static_cast<T *>(storage)->~T();
}

template <typename Type, typename... Types>
void Variant<Type, Types...>::destroy(std::size_t index,
                                      void *storage) noexcept {
  static void (*const destroyFuncs[])(void *) = {&variantDestroy<Type>,
                                                 &variantDestroy<Types>...};

  destroyFuncs[index](storage);
}

template <typename Type, typename... Types>
template <typename T>
constexpr std::size_t Variant<Type, Types...>::indexOfType() {
  return IndexHelper<Type, Types...>::template indexOf<T>();
}

template <typename Type, typename... Types>
template <typename T>
constexpr bool Variant<Type, Types...>::isValidType() {
  return indexOfType<T>() < (sizeof...(Types) + 1);
}

template <typename Type, typename... Types>
bool operator!=(const Variant<Type, Types...> &lhs,
                const Variant<Type, Types...> &rhs) {
  return !(lhs == rhs);
}

template <typename Type, typename... Types, typename>
std::ostream &operator<<(std::ostream &os,
                         const Variant<Type, Types...> &value) {
  value.printTo(os);
  return os;
}

} // namespace detail
} // namespace rc
