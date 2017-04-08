#pragma once

namespace rc {
namespace gen {
namespace detail {

template <typename Iterator>
class ShrinkValueIterator
    : public std::iterator<
          std::input_iterator_tag,
          typename std::iterator_traits<Iterator>::value_type::ValueType,
          std::ptrdiff_t,
          typename std::iterator_traits<Iterator>::value_type::ValueType *,
          typename std::iterator_traits<Iterator>::value_type::ValueType &&> {
public:
  using T = typename std::iterator_traits<Iterator>::value_type::ValueType;

  ShrinkValueIterator(Iterator it)
      : m_it(it) {}

  bool operator==(const ShrinkValueIterator &rhs) const {
    return m_it == rhs.m_it;
  }

  T operator*() const { return m_it->value(); }

  ShrinkValueIterator &operator++() {
    ++m_it;
    return *this;
  }

  ShrinkValueIterator operator++(int) {
    auto pre = m_it;
    ++m_it;
    return pre;
  }

private:
  Iterator m_it;
};

template <typename Iterator>
bool operator!=(const ShrinkValueIterator<Iterator> &lhs,
                const ShrinkValueIterator<Iterator> &rhs) {
  return !(lhs == rhs);
}

template <typename Iterator>
ShrinkValueIterator<Iterator> makeShrinkValueIterator(Iterator it) {
  return ShrinkValueIterator<Iterator>(it);
}

} // namespace detail
} // namespace gen
} // namespace rc
