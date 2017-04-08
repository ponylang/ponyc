#pragma once

#include "rapidcheck/Gen.h"

namespace rc {
namespace gen {

/// Generator of text characters. Common occuring characters have a higher
/// probability of being generated.
template <typename T>
Gen<T> character();

/// Generator of strings. Essentially equivalent to
/// `gen::container<String>(gen::character<typename String::value_type>())` but
/// a lot faster. If you need to use a custom character generator, use
/// `gen::container`.
template <typename String>
Gen<String> string();

} // namespace gen
} // namespace rc

#include "Text.hpp"
