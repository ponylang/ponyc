#pragma once

#include <vector>

namespace rc {
namespace detail {

/// Helper class for implementing weighted random generators. Using a table of
/// weights, provides a fast lookup of a value in the range `[0, sum(weights))`
/// to an index into the weights table.
class FrequencyMap {
public:
  explicit FrequencyMap(const std::vector<std::size_t> &frequencies);

  std::size_t lookup(std::size_t x) const;
  std::size_t sum() const;

private:
  std::size_t m_sum;
  std::vector<std::size_t> m_table;
};

} // namespace detail
} // namespace rc
