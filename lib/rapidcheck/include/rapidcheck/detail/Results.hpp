#pragma once

namespace rc {
namespace detail {

template <typename Iterator>
Iterator serialize(const Reproduce &value, Iterator output) {
  auto oit = output;
  oit = serialize(value.random, oit);
  oit = serialize(static_cast<std::uint32_t>(value.size), oit);
  oit = serializeCompact(begin(value.shrinkPath), end(value.shrinkPath), oit);
  return oit;
}

template <typename Iterator>
Iterator deserialize(Iterator begin, Iterator end, Reproduce &out) {
  auto iit = begin;
  iit = deserialize(iit, end, out.random);

  std::uint32_t size;
  iit = deserialize(iit, end, size);
  out.size = static_cast<int>(size);

  out.shrinkPath.clear();
  const auto p = deserializeCompact<std::size_t>(
      iit, end, std::back_inserter(out.shrinkPath));

  return p.first;
}

} // namespace detail
} // namespace rc
