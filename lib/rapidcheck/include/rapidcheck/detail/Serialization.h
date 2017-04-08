#pragma once

#include <stdexcept>
#include <string>
#include <unordered_map>

namespace rc {
namespace detail {

/// Thrown by serialization functions on failure.
class SerializationException : public std::exception {
public:
  /// C-tor.
  ///
  /// @param msg  A message describing the serialization error.
  SerializationException(const std::string &msg);

  /// Returns the message.
  std::string message() const;

  const char *what() const noexcept override;

private:
  std::string m_msg;
};

/// Serializes the given integer value in little-endian format.
template <typename T,
          typename Iterator,
          typename = typename std::enable_if<std::is_integral<T>::value>::type>
Iterator serialize(T value, Iterator output);

/// Deserializes integers as serialized by `serialize`.
template <typename T,
          typename Iterator,
          typename = typename std::enable_if<std::is_integral<T>::value>::type>
Iterator deserialize(Iterator begin, Iterator end, T &out);

/// Serializes an `std::string`.
template <typename Iterator>
Iterator serialize(const std::string &value, Iterator output);

/// Deserializes an `std::string`.
template <typename Iterator>
Iterator deserialize(Iterator begin, Iterator end, std::string &output);

/// Serializes an `std::pair`.
template <typename T1, typename T2, typename Iterator>
Iterator serialize(const std::pair<T1, T2> &value, Iterator output);

/// Deserializes an `std::pair`.
template <typename T1, typename T2, typename Iterator>
Iterator deserialize(Iterator begin, Iterator end, std::pair<T1, T2> &output);

/// Serializes an `std::map`-like container.
template <typename Map, typename Iterator>
Iterator serializeMap(const Map &value, Iterator output);

/// Deserializes an `std::map`-like container.
template <typename Map, typename Iterator>
Iterator deserializeMap(Iterator begin, Iterator end, Map &output);

/// Serializes an `std::unordered_map`.
template <typename Key,
          typename T,
          typename Hash,
          typename KeyEqual,
          typename Allocator,
          typename Iterator>
Iterator
serialize(const std::unordered_map<Key, T, Hash, KeyEqual, Allocator> &value,
          Iterator output);

/// Deserializes an `std::unordered_map`.
template <typename Key,
          typename T,
          typename Hash,
          typename KeyEqual,
          typename Allocator,
          typename Iterator>
Iterator
deserialize(Iterator begin,
            Iterator end,
            std::unordered_map<Key, T, Hash, KeyEqual, Allocator> &output);

/// Serializes `n` number of elements from `in` without storing the length.
/// Thus to deserialize, the exact number of elements must be known beforehand.
template <typename InputIterator, typename OutputIterator>
OutputIterator serializeN(InputIterator in, std::size_t n, OutputIterator out);

/// Deserializes `n` number of elements of type `T`.
template <typename T, typename InputIterator, typename OutputIterator>
InputIterator deserializeN(InputIterator begin,
                           InputIterator end,
                           std::size_t n,
                           OutputIterator out);

/// Serializes the given integer value in a compact form where only as many bits
/// as needed are used.
///
/// This is done by using only seven bits in each byte. When high bit is set, it
/// indicates that there are more bytes to read. This format is only useful if
/// the number of bits required is usually low. If the higher bits are usually
/// set, it will lead to inefficient storage.
template <typename T, typename Iterator>
Iterator serializeCompact(T value, Iterator output);

/// Deserializes integers as serialized by `serializeCompact`.
template <typename T, typename Iterator>
Iterator deserializeCompact(Iterator begin, Iterator end, T &output);

/// Serializes the given range of integers to the given output iterator. The
/// output is serialized using `serializeCompact` with a prefix length also
/// serialized the same way.
template <typename InputIterator, typename OutputIterator>
OutputIterator
serializeCompact(InputIterator begin, InputIterator end, OutputIterator output);

/// Deserializes a range of integers as serialized by the range version of
/// `serializeCompact`.
///
/// @return A pair of iterators pointing past the end of the consumed data and
///         past the end of the output data, respectively.
template <typename T, typename InputIterator, typename OutputIterator>
std::pair<InputIterator, OutputIterator> deserializeCompact(
    InputIterator begin, InputIterator end, OutputIterator output);

} // namespace detail
} // namespace rc

#include "Serialization.hpp"
