#pragma once

#include <cstdint>

namespace rc {
namespace detail {

/// This is a helper class to optimize the number of random numbers requested
/// from a random source. The random source must have a method `next()` that
/// returns a random number.
template <typename Source>
class BitStream {
public:
  explicit BitStream(Source source);

  /// Returns the next value of the given type with maximum size.
  template <typename T>
  T next();

  /// Returns the next random of the given type and number of bits.
  template <typename T>
  T next(int nbits);

  /// Returns the next random of the given type and size. Size maxes out at
  /// `kNominalSize`.
  template <typename T>
  T nextWithSize(int size);

private:
  template <typename T>
  T next(int nbits, std::true_type);

  template <typename T>
  T next(int nbits, std::false_type);

  Source m_source;
  uint64_t m_bits;
  int m_numBits;
};

/// Returns a bitstream with the given source as a reference.
template <typename Source>
BitStream<Source &> bitStreamOf(Source &source);

/// Returns a bitstream with the given source as a copy.
template <typename Source>
BitStream<Source> bitStreamOf(const Source &source);

} // namespace detail
} // namespace rc

#include "BitStream.hpp"
