trait Random
  // If used as an iterator, this always has another value.
  fun tag has_next(): Bool => true

  // A random integer in [0, 2^64)
  fun ref next(): U64

  // A random integer in [0, 2^8)
  fun ref u8(): U8 => (next() >> 56).u8()

  // A random integer in [0, 2^16)
  fun ref u16(): U16 => (next() >> 48).u16()

  // A random integer in [0, 2^32)
  fun ref u32(): U32 => (next() >> 32).u32()

  // A random integer in [0, 2^64)
  fun ref u64(): U64 => next()

  // A random integer in [0, 2^128)
  fun ref u128(): U128 => (next().u128() << 64) or next().u128()

  // A random integer in [0, n)
  fun ref int(n: U64): U64 => (real() * n.f64()).u64()

  // A random number in [0, 1)
  fun ref real(): F64 => (next() >> 11).f64() * (F64(1) / 9007199254740992)
