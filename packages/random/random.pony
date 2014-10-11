trait Random
  // If used as an iterator, this always has another value.
  fun tag has_next(): Bool => true

  // A random integer in [0, 2^64 - 1]
  fun ref next(): U64

  // A random number in [0, 1)
  fun ref real(): F64
