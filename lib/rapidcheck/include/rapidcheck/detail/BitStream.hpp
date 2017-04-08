#pragma once

#include <limits>

#include "rapidcheck/detail/Utility.h"

namespace rc {
namespace detail {

template <typename T>
constexpr int numBits() {
  return std::numeric_limits<T>::digits + (std::is_signed<T>::value ? 1 : 0);
}

template <typename Source>
BitStream<Source>::BitStream(Source source)
    : m_source(source)
    , m_bits(0)
    , m_numBits(0) {}

template <typename Source>
template <typename T>
T BitStream<Source>::next() {
  return next<T>(numBits<T>());
}

template <typename Source>
template <typename T>
T BitStream<Source>::next(int nbits) {
  return next<T>(nbits, std::is_same<T, bool>());
}

template <typename Source>
template <typename T>
T BitStream<Source>::next(int /*nbits*/, std::true_type) {
  return next<unsigned int>(1) != 0;
}

template <typename Source>
template <typename T>
T BitStream<Source>::next(int nbits, std::false_type) {
  using SourceType = decltype(m_source.next());

  if (nbits == 0) {
    return 0;
  }

  T value = 0;
  int wantBits = nbits;
  while (wantBits > 0) {
    // Out of bits, refill
    if (m_numBits == 0) {
      m_bits = m_source.next();
      m_numBits += numBits<SourceType>();
    }

    const auto n = std::min(m_numBits, wantBits);
    const auto bits = m_bits & bitMask<SourceType>(n);
    value |= (bits << (nbits - wantBits));
    // To avoid right-shifting data beyond the width of the given type (which is
    // undefined behavior, because of course it is) only perform this shift-
    // assignment if we have room.
    if (static_cast<SourceType>(n) < numBits<SourceType>()) {
      m_bits >>= static_cast<SourceType>(n);
    }
    m_numBits -= n;
    wantBits -= n;
  }

  if (std::is_signed<T>::value) {
    T signBit = static_cast<T>(0x1) << static_cast<T>(nbits - 1);
    if ((value & signBit) != 0) {
      // For signed values, we use the last bit as the sign bit. Since this
      // was 1, mask away by setting all bits above this one to 1 to make it a
      // negative number in 2's complement
      value |= ~bitMask<T>(nbits);
    }
  }

  return value;
}

template <typename Source>
template <typename T>
T BitStream<Source>::nextWithSize(int size) {
  return next<T>((size * numBits<T>() + (kNominalSize / 2)) / kNominalSize);
}

template <typename Source>
BitStream<Source &> bitStreamOf(Source &source) {
  return BitStream<Source &>(source);
}

/// Returns a bitstream with the given source as a copy.
template <typename Source>
BitStream<Source> bitStreamOf(const Source &source) {
  return BitStream<Source>(source);
}

} // namespace detail
} // namespace rc
