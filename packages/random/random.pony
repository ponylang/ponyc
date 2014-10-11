trait Random
  // If used as an iterator, this always has another value.
  fun tag has_next(): Bool => true

  // A random integer in [0, 2^64)
  fun ref next(): U64

  // A random integer in [0, n)
  fun ref int(n: U64): U64 =>
    (real() * n.f64()).u64()

  // A random number in [0, 1)
  fun ref real(): F64 =>
    (next() >> 11).f64() * (F64(1) / 9007199254740992)
