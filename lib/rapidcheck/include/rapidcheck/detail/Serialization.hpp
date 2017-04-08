#include "Serialization.h"

#include <limits>

namespace rc {
namespace detail {

template <typename T, typename Iterator, typename>
Iterator serialize(T value, Iterator output) {
  using UInt = typename std::make_unsigned<T>::type;
  constexpr auto nbytes = std::numeric_limits<UInt>::digits / 8;

  const auto uvalue = static_cast<UInt>(value);
  auto it = output;
  for (std::size_t i = 0; i < nbytes; i++) {
    *it = static_cast<std::uint8_t>(uvalue >> (i * 8));
    it++;
  }

  return it;
}

template <typename T, typename Iterator, typename>
Iterator deserialize(Iterator begin, Iterator end, T &out) {
  using UInt = typename std::make_unsigned<T>::type;
  constexpr auto nbytes = std::numeric_limits<UInt>::digits / 8;

  UInt uvalue = 0;
  auto it = begin;
  for (std::size_t i = 0; i < nbytes; i++) {
    if (it == end) {
      throw SerializationException("Unexpected end of input");
    }

    uvalue |= static_cast<UInt>(*it & 0xFF) << (i * 8);
    it++;
  }

  out = static_cast<T>(uvalue);
  return it;
}

template <typename Iterator>
Iterator serialize(const std::string &value, Iterator output) {
  auto oit = output;
  oit = serializeCompact(value.size(), oit);
  return std::copy(begin(value), end(value), oit);
}

template <typename Iterator>
Iterator deserialize(Iterator begin, Iterator end, std::string &output) {
  auto iit = begin;
  std::size_t len;
  iit = deserializeCompact(iit, end, len);

  output.clear();
  output.reserve(len);
  while (output.size() < len) {
    if (iit == end) {
      throw SerializationException("Unexpected end of input");
    }

    output.push_back(*iit);
    iit++;
  }

  return iit;
}
template <typename T1, typename T2, typename Iterator>
Iterator serialize(const std::pair<T1, T2> &value, Iterator output) {
  auto oit = output;
  oit = serialize(value.first, oit);
  oit = serialize(value.second, oit);
  return oit;
}

template <typename T1, typename T2, typename Iterator>
Iterator deserialize(Iterator begin, Iterator end, std::pair<T1, T2> &output) {
  auto iit = begin;
  iit = deserialize(iit, end, output.first);
  iit = deserialize(iit, end, output.second);
  return iit;
}

template <typename Map, typename Iterator>
Iterator serializeMap(const Map &value, Iterator output) {
  auto oit = output;
  oit = serializeCompact(value.size(), oit);
  for (const auto &p : value) {
    oit = serialize(p, oit);
  }

  return oit;
}

template <typename Map, typename Iterator>
Iterator deserializeMap(Iterator begin, Iterator end, Map &output) {
  auto iit = begin;
  std::size_t len;
  iit = deserializeCompact(iit, end, len);
  output.clear();
  while (output.size() < len) {
    using Key = typename Map::key_type;
    using Value = typename Map::mapped_type;
    std::pair<Key, Value> element;
    iit = deserialize(iit, end, element);
    output.insert(std::move(element));
  }

  return iit;
}

template <typename Key,
          typename T,
          typename Hash,
          typename KeyEqual,
          typename Allocator,
          typename Iterator>
Iterator
serialize(const std::unordered_map<Key, T, Hash, KeyEqual, Allocator> &value,
          Iterator output) {
  return serializeMap(value, output);
}

template <typename Key,
          typename T,
          typename Hash,
          typename KeyEqual,
          typename Allocator,
          typename Iterator>
Iterator
deserialize(Iterator begin,
            Iterator end,
            std::unordered_map<Key, T, Hash, KeyEqual, Allocator> &output) {
  return deserializeMap(begin, end, output);
}

template <typename InputIterator, typename OutputIterator>
OutputIterator serializeN(InputIterator in, std::size_t n, OutputIterator out) {
  auto oit = out;
  auto iit = in;
  for (std::size_t i = 0; i < n; i++) {
    oit = serialize(*iit, oit);
    iit++;
  }

  return oit;
}

template <typename T, typename InputIterator, typename OutputIterator>
InputIterator deserializeN(InputIterator begin,
                           InputIterator end,
                           std::size_t n,
                           OutputIterator out) {
  auto iit = begin;
  auto oit = out;
  for (std::size_t i = 0; i < n; i++) {
    T value;
    iit = deserialize(iit, end, value);
    *oit = std::move(value);
    oit++;
  }

  return iit;
}

template <typename T, typename Iterator>
Iterator serializeCompact(T value, Iterator output) {
  static_assert(std::is_integral<T>::value, "T must be integral");

  using UInt = typename std::make_unsigned<T>::type;
  auto uvalue = static_cast<UInt>(value);
  do {
    auto x = uvalue & 0x7F;
    uvalue = uvalue >> 7;
    *output = static_cast<std::uint8_t>((uvalue == 0) ? x : (x | 0x80));
    output++;
  } while (uvalue != 0);

  return output;
}

template <typename T, typename Iterator>
Iterator deserializeCompact(Iterator begin, Iterator end, T &output) {
  static_assert(std::is_integral<T>::value, "T must be integral");
  using UInt = typename std::make_unsigned<T>::type;

  UInt uvalue = 0;
  int nbits = 0;
  for (auto it = begin; it != end; it++) {
    auto x = static_cast<std::uint8_t>(*it);
    uvalue |= static_cast<UInt>(x & 0x7F) << nbits;
    nbits += 7;
    if ((x & 0x80) == 0) {
      output = static_cast<T>(uvalue);
      return ++it;
    }
  }

  throw SerializationException("Unexpected end of input");
}

template <typename InputIterator, typename OutputIterator>
OutputIterator
serializeCompact(InputIterator begin, InputIterator end, OutputIterator output) {
  const std::uint64_t numElements = std::distance(begin, end);
  auto oit = serializeCompact(numElements, output);
  for (auto it = begin; it != end; it++) {
    oit = serializeCompact(*it, oit);
  }

  return oit;
}

template <typename T, typename InputIterator, typename OutputIterator>
std::pair<InputIterator, OutputIterator> deserializeCompact(
    InputIterator begin, InputIterator end, OutputIterator output) {
  std::uint64_t numElements;
  auto iit = deserializeCompact(begin, end, numElements);
  auto oit = output;
  for (std::uint64_t i = 0; i < numElements; i++) {
    T value;
    iit = deserializeCompact(iit, end, value);
    *oit = value;
    oit++;
  }

  return std::make_pair(iit, oit);
}

} // namespace detail
} // namespace rc
