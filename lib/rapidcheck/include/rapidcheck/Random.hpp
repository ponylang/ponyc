#pragma once

#include "rapidcheck/detail/Serialization.h"

namespace rc {

template <typename Iterator>
Iterator serialize(const Random &random, Iterator output) {
  using namespace rc::detail;
  auto oit = output;
  oit = serializeN(begin(random.m_key), random.m_key.size(), oit);
  oit = serializeCompact(random.m_bits, oit);
  oit = serializeCompact(random.m_counter, oit);
  *oit = random.m_bitsi;
  return ++oit;
}

template <typename Iterator>
Iterator deserialize(Iterator begin, Iterator end, Random &output) {
  using namespace rc::detail;
  auto iit = begin;

  iit = deserializeN<typename Random::Block::value_type>(
      iit, end, output.m_key.size(), output.m_key.begin());
  iit = deserializeCompact(iit, end, output.m_bits);

  Random::Counter counter;
  iit = deserializeCompact(iit, end, counter);
  // Normally, the block is calculated lazily if counter is divisible by 4 so
  // let's simulate this.
  if (counter != 0) {
    const auto blki =
        ((counter - 1) % std::tuple_size<Random::Block>::value) + 1;
    if (blki != 0) {
      // Calculate the block as if counter % 4 == 0
      output.m_counter = counter - blki;
      output.mash(output.m_block);
    }
  }
  output.m_counter = counter;

  output.m_bitsi = *iit;
  return ++iit;
}

} // namespace rc
