#pragma once

#include <cstdint>
#include <array>
#include <limits>

namespace rc {

/// Implementation of a splittable random generator as described in:
///   Claessen, K. och Palka, M. (2013) Splittable Pseudorandom Number
///   Generators using Cryptographic Hashing.
class Random {
  friend bool operator==(const Random &lhs, const Random &rhs);
  friend bool operator<(const Random &lhs, const Random &rhs);
  friend std::ostream &operator<<(std::ostream &os, const Random &random);

  template <typename Iterator>
  friend Iterator serialize(const Random &random, Iterator output);

  template <typename Iterator>
  friend Iterator deserialize(Iterator begin, Iterator end, Random &output);

public:
  /// Key type
  using Key = std::array<uint64_t, 4>;

  /// Type of a generated random number.
  using Number = uint64_t;

  /// Constructs a Random engine with a `{0, 0, 0, 0}` key.
  Random();

  /// Constructs a Random engine from a full size 256-bit key.
  Random(const Key &key);

  /// Constructs a Random engine from a 64-bit seed.
  Random(uint64_t seed);

  /// Splits this generator into to separate independent generators. The first
  /// generator will be assigned to this one and the second will be returned.
  Random split();

  /// Returns the next random number. Both `split` and `next` should not be
  /// called on the same state.
  Number next();

private:
  using Block = std::array<uint64_t, 4>;

  using Bits = uint64_t;
  static constexpr auto kBits = std::numeric_limits<Bits>::digits;

  using Counter = uint64_t;
  static constexpr auto kCounterMax = std::numeric_limits<Counter>::max();

  void append(bool x);
  void mash(Block &output);

  Block m_key;
  Block m_block;
  Bits m_bits;
  Counter m_counter;
  uint8_t m_bitsi;
};

bool operator!=(const Random &lhs, const Random &rhs);

} // namespace rc

namespace std {

template <>
struct hash<rc::Random> {
  using argument_type = rc::Random;
  using result_type = std::size_t;

  std::size_t operator()(const rc::Random &r) const {
    return static_cast<std::size_t>(rc::Random(r).next());
  }
};

} // namespace std

#include "Random.hpp"
