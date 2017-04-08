#pragma once

namespace rc {
namespace seq {

template <typename T>
SeqIterator<T>::SeqIterator(Seq<T> seq)
    : m_seq(std::move(seq))
    , m_current(m_seq.next()) {}

template <typename T>
bool SeqIterator<T>::operator==(const SeqIterator<T> &rhs) const {
  return !m_current && !rhs.m_current;
}

template <typename T>
T &SeqIterator<T>::operator*() {
  return *m_current;
}

template <typename T>
const T &SeqIterator<T>::operator*() const {
  return *m_current;
}

template <typename T>
SeqIterator<T> &SeqIterator<T>::operator++() {
  m_current = m_seq.next();
  return *this;
}

template <typename T>
SeqIterator<T> SeqIterator<T>::operator++(int) {
  SeqIterator it = *this;
  ++(*this);
  return it;
}

template <typename T>
bool operator!=(const SeqIterator<T> &lhs, const SeqIterator<T> &rhs) {
  return !(lhs == rhs);
}

} // namespace seq

template <typename T>
seq::SeqIterator<T> begin(Seq<T> seq) {
  return seq::SeqIterator<T>(std::move(seq));
}

template <typename T>
seq::SeqIterator<T> end(const Seq<T> &seq) {
  return seq::SeqIterator<T>();
}

} // namespace rc
