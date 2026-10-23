## Fix collections.List stack overflow on large lists in debug builds

List's `map`, `flat_map`, `filter`, `fold`, `every`, `exists`, `reverse`, and `contains` methods used recursive helpers that overflowed the stack in debug builds at around 100,000 elements. Release builds were unaffected because LLVM optimized the tail calls into loops. These methods now use iterative loops unconditionally.
