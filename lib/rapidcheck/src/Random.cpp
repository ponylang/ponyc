#include "rapidcheck/Random.h"

#include <iostream>
#include <cassert>
#include <functional>

#include "rapidcheck/Show.h"

// A lot of this code is taken from https://github.com/wernerd/Skein3Fish but
// highly modified.

namespace rc {
namespace {

constexpr uint64_t kKeyScheduleParity = 0x1BD11BDAA9FC1A22ULL;
constexpr uint64_t kTweak[2] = {13, 37};
}

Random::Random()
    : Random(Key{{0, 0, 0, 0}}) {}

Random::Random(const Key &key)
    : m_key(key)
    , m_block()
    , m_bits(0)
    , m_counter(0)
    , m_bitsi(0) {}

// We just repeat the seed in the key
Random::Random(uint64_t seed)
    : Random(Key{{seed, seed, seed, seed}}) {}

Random Random::split() {
  assert(m_counter == 0);
  Random right(*this);
  append(false);
  right.append(true);
  return right;
}

Random::Number Random::next() {
  std::size_t blki = m_counter % std::tuple_size<Block>::value;
  if (blki == 0) {
    mash(m_block);
  }

  if (m_counter == kCounterMax) {
    append(true);
    m_counter = 0;
  } else {
    m_counter++;
  }

  return m_block[blki];
}

void Random::append(bool x) {
  if (m_bitsi == kBits) {
    mash(m_key);
    m_bitsi = 0;
    m_bits = 0;
  }

  if (x) {
    m_bits |= (0x1ULL << m_bitsi);
  }
  m_bitsi++;
}

void Random::mash(Block &output) {
  // Input
  uint64_t b0 = m_bits;
  uint64_t b1 = m_counter;
  uint64_t b2 = 0;
  uint64_t b3 = 0;

  uint64_t k0 = m_key[0];
  uint64_t k1 = m_key[1];
  uint64_t k2 = m_key[2];
  uint64_t k3 = m_key[3];
  uint64_t k4 = k0 ^ k1 ^ k2 ^ k3 ^ kKeyScheduleParity;

  uint64_t t0 = kTweak[0];
  uint64_t t1 = kTweak[1];
  uint64_t t2 = t0 ^ t1;

  b1 += k1 + t0;
  b0 += b1 + k0;
  b1 = ((b1 << 14) | (b1 >> (64 - 14))) ^ b0;
  b3 += k3;
  b2 += b3 + k2 + t1;
  b3 = ((b3 << 16) | (b3 >> (64 - 16))) ^ b2;
  b0 += b3;
  b3 = ((b3 << 52) | (b3 >> (64 - 52))) ^ b0;
  b2 += b1;
  b1 = ((b1 << 57) | (b1 >> (64 - 57))) ^ b2;
  b0 += b1;
  b1 = ((b1 << 23) | (b1 >> (64 - 23))) ^ b0;
  b2 += b3;
  b3 = ((b3 << 40) | (b3 >> (64 - 40))) ^ b2;
  b0 += b3;
  b3 = ((b3 << 5) | (b3 >> (64 - 5))) ^ b0;
  b2 += b1;
  b1 = ((b1 << 37) | (b1 >> (64 - 37))) ^ b2;
  b1 += k2 + t1;
  b0 += b1 + k1;
  b1 = ((b1 << 25) | (b1 >> (64 - 25))) ^ b0;
  b3 += k4 + 1;
  b2 += b3 + k3 + t2;
  b3 = ((b3 << 33) | (b3 >> (64 - 33))) ^ b2;
  b0 += b3;
  b3 = ((b3 << 46) | (b3 >> (64 - 46))) ^ b0;
  b2 += b1;
  b1 = ((b1 << 12) | (b1 >> (64 - 12))) ^ b2;
  b0 += b1;
  b1 = ((b1 << 58) | (b1 >> (64 - 58))) ^ b0;
  b2 += b3;
  b3 = ((b3 << 22) | (b3 >> (64 - 22))) ^ b2;
  b0 += b3;
  b3 = ((b3 << 32) | (b3 >> (64 - 32))) ^ b0;
  b2 += b1;
  b1 = ((b1 << 32) | (b1 >> (64 - 32))) ^ b2;

  b1 += k3 + t2;
  b0 += b1 + k2;
  b1 = ((b1 << 14) | (b1 >> (64 - 14))) ^ b0;
  b3 += k0 + 2;
  b2 += b3 + k4 + t0;
  b3 = ((b3 << 16) | (b3 >> (64 - 16))) ^ b2;
  b0 += b3;
  b3 = ((b3 << 52) | (b3 >> (64 - 52))) ^ b0;
  b2 += b1;
  b1 = ((b1 << 57) | (b1 >> (64 - 57))) ^ b2;
  b0 += b1;
  b1 = ((b1 << 23) | (b1 >> (64 - 23))) ^ b0;
  b2 += b3;
  b3 = ((b3 << 40) | (b3 >> (64 - 40))) ^ b2;
  b0 += b3;
  b3 = ((b3 << 5) | (b3 >> (64 - 5))) ^ b0;
  b2 += b1;
  b1 = ((b1 << 37) | (b1 >> (64 - 37))) ^ b2;
  b1 += k4 + t0;
  b0 += b1 + k3;
  b1 = ((b1 << 25) | (b1 >> (64 - 25))) ^ b0;
  b3 += k1 + 3;
  b2 += b3 + k0 + t1;
  b3 = ((b3 << 33) | (b3 >> (64 - 33))) ^ b2;
  b0 += b3;
  b3 = ((b3 << 46) | (b3 >> (64 - 46))) ^ b0;
  b2 += b1;
  b1 = ((b1 << 12) | (b1 >> (64 - 12))) ^ b2;
  b0 += b1;
  b1 = ((b1 << 58) | (b1 >> (64 - 58))) ^ b0;
  b2 += b3;
  b3 = ((b3 << 22) | (b3 >> (64 - 22))) ^ b2;
  b0 += b3;
  b3 = ((b3 << 32) | (b3 >> (64 - 32))) ^ b0;
  b2 += b1;
  b1 = ((b1 << 32) | (b1 >> (64 - 32))) ^ b2;

  b1 += k0 + t1;
  b0 += b1 + k4;
  b1 = ((b1 << 14) | (b1 >> (64 - 14))) ^ b0;
  b3 += k2 + 4;
  b2 += b3 + k1 + t2;
  b3 = ((b3 << 16) | (b3 >> (64 - 16))) ^ b2;
  b0 += b3;
  b3 = ((b3 << 52) | (b3 >> (64 - 52))) ^ b0;
  b2 += b1;
  b1 = ((b1 << 57) | (b1 >> (64 - 57))) ^ b2;
  b0 += b1;
  b1 = ((b1 << 23) | (b1 >> (64 - 23))) ^ b0;
  b2 += b3;
  b3 = ((b3 << 40) | (b3 >> (64 - 40))) ^ b2;
  b0 += b3;
  b3 = ((b3 << 5) | (b3 >> (64 - 5))) ^ b0;
  b2 += b1;
  b1 = ((b1 << 37) | (b1 >> (64 - 37))) ^ b2;
  b1 += k1 + t2;
  b0 += b1 + k0;
  b1 = ((b1 << 25) | (b1 >> (64 - 25))) ^ b0;
  b3 += k3 + 5;
  b2 += b3 + k2 + t0;
  b3 = ((b3 << 33) | (b3 >> (64 - 33))) ^ b2;
  b0 += b3;
  b3 = ((b3 << 46) | (b3 >> (64 - 46))) ^ b0;
  b2 += b1;
  b1 = ((b1 << 12) | (b1 >> (64 - 12))) ^ b2;
  b0 += b1;
  b1 = ((b1 << 58) | (b1 >> (64 - 58))) ^ b0;
  b2 += b3;
  b3 = ((b3 << 22) | (b3 >> (64 - 22))) ^ b2;
  b0 += b3;
  b3 = ((b3 << 32) | (b3 >> (64 - 32))) ^ b0;
  b2 += b1;
  b1 = ((b1 << 32) | (b1 >> (64 - 32))) ^ b2;

  b1 += k2 + t0;
  b0 += b1 + k1;
  b1 = ((b1 << 14) | (b1 >> (64 - 14))) ^ b0;
  b3 += k4 + 6;
  b2 += b3 + k3 + t1;
  b3 = ((b3 << 16) | (b3 >> (64 - 16))) ^ b2;
  b0 += b3;
  b3 = ((b3 << 52) | (b3 >> (64 - 52))) ^ b0;
  b2 += b1;
  b1 = ((b1 << 57) | (b1 >> (64 - 57))) ^ b2;
  b0 += b1;
  b1 = ((b1 << 23) | (b1 >> (64 - 23))) ^ b0;
  b2 += b3;
  b3 = ((b3 << 40) | (b3 >> (64 - 40))) ^ b2;
  b0 += b3;
  b3 = ((b3 << 5) | (b3 >> (64 - 5))) ^ b0;
  b2 += b1;
  b1 = ((b1 << 37) | (b1 >> (64 - 37))) ^ b2;
  b1 += k3 + t1;
  b0 += b1 + k2;
  b1 = ((b1 << 25) | (b1 >> (64 - 25))) ^ b0;
  b3 += k0 + 7;
  b2 += b3 + k4 + t2;
  b3 = ((b3 << 33) | (b3 >> (64 - 33))) ^ b2;
  b0 += b3;
  b3 = ((b3 << 46) | (b3 >> (64 - 46))) ^ b0;
  b2 += b1;
  b1 = ((b1 << 12) | (b1 >> (64 - 12))) ^ b2;
  b0 += b1;
  b1 = ((b1 << 58) | (b1 >> (64 - 58))) ^ b0;
  b2 += b3;
  b3 = ((b3 << 22) | (b3 >> (64 - 22))) ^ b2;
  b0 += b3;
  b3 = ((b3 << 32) | (b3 >> (64 - 32))) ^ b0;
  b2 += b1;
  b1 = ((b1 << 32) | (b1 >> (64 - 32))) ^ b2;

  b1 += k4 + t2;
  b0 += b1 + k3;
  b1 = ((b1 << 14) | (b1 >> (64 - 14))) ^ b0;
  b3 += k1 + 8;
  b2 += b3 + k0 + t0;
  b3 = ((b3 << 16) | (b3 >> (64 - 16))) ^ b2;
  b0 += b3;
  b3 = ((b3 << 52) | (b3 >> (64 - 52))) ^ b0;
  b2 += b1;
  b1 = ((b1 << 57) | (b1 >> (64 - 57))) ^ b2;
  b0 += b1;
  b1 = ((b1 << 23) | (b1 >> (64 - 23))) ^ b0;
  b2 += b3;
  b3 = ((b3 << 40) | (b3 >> (64 - 40))) ^ b2;
  b0 += b3;
  b3 = ((b3 << 5) | (b3 >> (64 - 5))) ^ b0;
  b2 += b1;
  b1 = ((b1 << 37) | (b1 >> (64 - 37))) ^ b2;
  b1 += k0 + t0;
  b0 += b1 + k4;
  b1 = ((b1 << 25) | (b1 >> (64 - 25))) ^ b0;
  b3 += k2 + 9;
  b2 += b3 + k1 + t1;
  b3 = ((b3 << 33) | (b3 >> (64 - 33))) ^ b2;
  b0 += b3;
  b3 = ((b3 << 46) | (b3 >> (64 - 46))) ^ b0;
  b2 += b1;
  b1 = ((b1 << 12) | (b1 >> (64 - 12))) ^ b2;
  b0 += b1;
  b1 = ((b1 << 58) | (b1 >> (64 - 58))) ^ b0;
  b2 += b3;
  b3 = ((b3 << 22) | (b3 >> (64 - 22))) ^ b2;
  b0 += b3;
  b3 = ((b3 << 32) | (b3 >> (64 - 32))) ^ b0;
  b2 += b1;
  b1 = ((b1 << 32) | (b1 >> (64 - 32))) ^ b2;

  b1 += k1 + t1;
  b0 += b1 + k0;
  b1 = ((b1 << 14) | (b1 >> (64 - 14))) ^ b0;
  b3 += k3 + 10;
  b2 += b3 + k2 + t2;
  b3 = ((b3 << 16) | (b3 >> (64 - 16))) ^ b2;
  b0 += b3;
  b3 = ((b3 << 52) | (b3 >> (64 - 52))) ^ b0;
  b2 += b1;
  b1 = ((b1 << 57) | (b1 >> (64 - 57))) ^ b2;
  b0 += b1;
  b1 = ((b1 << 23) | (b1 >> (64 - 23))) ^ b0;
  b2 += b3;
  b3 = ((b3 << 40) | (b3 >> (64 - 40))) ^ b2;
  b0 += b3;
  b3 = ((b3 << 5) | (b3 >> (64 - 5))) ^ b0;
  b2 += b1;
  b1 = ((b1 << 37) | (b1 >> (64 - 37))) ^ b2;
  b1 += k2 + t2;
  b0 += b1 + k1;
  b1 = ((b1 << 25) | (b1 >> (64 - 25))) ^ b0;
  b3 += k4 + 11;
  b2 += b3 + k3 + t0;
  b3 = ((b3 << 33) | (b3 >> (64 - 33))) ^ b2;
  b0 += b3;
  b3 = ((b3 << 46) | (b3 >> (64 - 46))) ^ b0;
  b2 += b1;
  b1 = ((b1 << 12) | (b1 >> (64 - 12))) ^ b2;
  b0 += b1;
  b1 = ((b1 << 58) | (b1 >> (64 - 58))) ^ b0;
  b2 += b3;
  b3 = ((b3 << 22) | (b3 >> (64 - 22))) ^ b2;
  b0 += b3;
  b3 = ((b3 << 32) | (b3 >> (64 - 32))) ^ b0;
  b2 += b1;
  b1 = ((b1 << 32) | (b1 >> (64 - 32))) ^ b2;

  b1 += k3 + t0;
  b0 += b1 + k2;
  b1 = ((b1 << 14) | (b1 >> (64 - 14))) ^ b0;
  b3 += k0 + 12;
  b2 += b3 + k4 + t1;
  b3 = ((b3 << 16) | (b3 >> (64 - 16))) ^ b2;
  b0 += b3;
  b3 = ((b3 << 52) | (b3 >> (64 - 52))) ^ b0;
  b2 += b1;
  b1 = ((b1 << 57) | (b1 >> (64 - 57))) ^ b2;
  b0 += b1;
  b1 = ((b1 << 23) | (b1 >> (64 - 23))) ^ b0;
  b2 += b3;
  b3 = ((b3 << 40) | (b3 >> (64 - 40))) ^ b2;
  b0 += b3;
  b3 = ((b3 << 5) | (b3 >> (64 - 5))) ^ b0;
  b2 += b1;
  b1 = ((b1 << 37) | (b1 >> (64 - 37))) ^ b2;
  b1 += k4 + t1;
  b0 += b1 + k3;
  b1 = ((b1 << 25) | (b1 >> (64 - 25))) ^ b0;
  b3 += k1 + 13;
  b2 += b3 + k0 + t2;
  b3 = ((b3 << 33) | (b3 >> (64 - 33))) ^ b2;
  b0 += b3;
  b3 = ((b3 << 46) | (b3 >> (64 - 46))) ^ b0;
  b2 += b1;
  b1 = ((b1 << 12) | (b1 >> (64 - 12))) ^ b2;
  b0 += b1;
  b1 = ((b1 << 58) | (b1 >> (64 - 58))) ^ b0;
  b2 += b3;
  b3 = ((b3 << 22) | (b3 >> (64 - 22))) ^ b2;
  b0 += b3;
  b3 = ((b3 << 32) | (b3 >> (64 - 32))) ^ b0;
  b2 += b1;
  b1 = ((b1 << 32) | (b1 >> (64 - 32))) ^ b2;

  b1 += k0 + t2;
  b0 += b1 + k4;
  b1 = ((b1 << 14) | (b1 >> (64 - 14))) ^ b0;
  b3 += k2 + 14;
  b2 += b3 + k1 + t0;
  b3 = ((b3 << 16) | (b3 >> (64 - 16))) ^ b2;
  b0 += b3;
  b3 = ((b3 << 52) | (b3 >> (64 - 52))) ^ b0;
  b2 += b1;
  b1 = ((b1 << 57) | (b1 >> (64 - 57))) ^ b2;
  b0 += b1;
  b1 = ((b1 << 23) | (b1 >> (64 - 23))) ^ b0;
  b2 += b3;
  b3 = ((b3 << 40) | (b3 >> (64 - 40))) ^ b2;
  b0 += b3;
  b3 = ((b3 << 5) | (b3 >> (64 - 5))) ^ b0;
  b2 += b1;
  b1 = ((b1 << 37) | (b1 >> (64 - 37))) ^ b2;
  b1 += k1 + t0;
  b0 += b1 + k0;
  b1 = ((b1 << 25) | (b1 >> (64 - 25))) ^ b0;
  b3 += k3 + 15;
  b2 += b3 + k2 + t1;
  b3 = ((b3 << 33) | (b3 >> (64 - 33))) ^ b2;
  b0 += b3;
  b3 = ((b3 << 46) | (b3 >> (64 - 46))) ^ b0;
  b2 += b1;
  b1 = ((b1 << 12) | (b1 >> (64 - 12))) ^ b2;
  b0 += b1;
  b1 = ((b1 << 58) | (b1 >> (64 - 58))) ^ b0;
  b2 += b3;
  b3 = ((b3 << 22) | (b3 >> (64 - 22))) ^ b2;
  b0 += b3;
  b3 = ((b3 << 32) | (b3 >> (64 - 32))) ^ b0;
  b2 += b1;
  b1 = ((b1 << 32) | (b1 >> (64 - 32))) ^ b2;

  b1 += k2 + t1;
  b0 += b1 + k1;
  b1 = ((b1 << 14) | (b1 >> (64 - 14))) ^ b0;
  b3 += k4 + 16;
  b2 += b3 + k3 + t2;
  b3 = ((b3 << 16) | (b3 >> (64 - 16))) ^ b2;
  b0 += b3;
  b3 = ((b3 << 52) | (b3 >> (64 - 52))) ^ b0;
  b2 += b1;
  b1 = ((b1 << 57) | (b1 >> (64 - 57))) ^ b2;
  b0 += b1;
  b1 = ((b1 << 23) | (b1 >> (64 - 23))) ^ b0;
  b2 += b3;
  b3 = ((b3 << 40) | (b3 >> (64 - 40))) ^ b2;
  b0 += b3;
  b3 = ((b3 << 5) | (b3 >> (64 - 5))) ^ b0;
  b2 += b1;
  b1 = ((b1 << 37) | (b1 >> (64 - 37))) ^ b2;
  b1 += k3 + t2;
  b0 += b1 + k2;
  b1 = ((b1 << 25) | (b1 >> (64 - 25))) ^ b0;
  b3 += k0 + 17;
  b2 += b3 + k4 + t0;
  b3 = ((b3 << 33) | (b3 >> (64 - 33))) ^ b2;
  b0 += b3;
  b3 = ((b3 << 46) | (b3 >> (64 - 46))) ^ b0;
  b2 += b1;
  b1 = ((b1 << 12) | (b1 >> (64 - 12))) ^ b2;
  b0 += b1;
  b1 = ((b1 << 58) | (b1 >> (64 - 58))) ^ b0;
  b2 += b3;
  b3 = ((b3 << 22) | (b3 >> (64 - 22))) ^ b2;
  b0 += b3;
  b3 = ((b3 << 32) | (b3 >> (64 - 32))) ^ b0;
  b2 += b1;
  b1 = ((b1 << 32) | (b1 >> (64 - 32))) ^ b2;

  // Output
  output[0] = b0 + k3;
  output[1] = b1 + k4 + t0;
  output[2] = b2 + k0 + t1;
  output[3] = b3 + k1 + 18;
}

bool operator==(const Random &lhs, const Random &rhs) {
  return (lhs.m_key == rhs.m_key) && (lhs.m_block == rhs.m_block) &&
      (lhs.m_bits == rhs.m_bits) && (lhs.m_counter == rhs.m_counter) &&
      (lhs.m_bitsi == rhs.m_bitsi);
}

bool operator!=(const Random &lhs, const Random &rhs) { return !(lhs == rhs); }

bool operator<(const Random &lhs, const Random &rhs) {
  return std::tie(
             lhs.m_key, lhs.m_block, lhs.m_bits, lhs.m_counter, lhs.m_bitsi) <
      std::tie(rhs.m_key, rhs.m_block, rhs.m_bits, rhs.m_counter, rhs.m_bitsi);
}

std::ostream &operator<<(std::ostream &os, const Random &random) {
  os << "key=";
  show(random.m_key, os);
  os << ", block=";
  show(random.m_block, os);
  os << ", bits=" << random.m_bits;
  os << ", counter=" << random.m_counter;
  os << ", bitsi=" << static_cast<int>(random.m_bitsi);
  return os;
}

} // namespace rc
