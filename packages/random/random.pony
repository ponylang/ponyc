"""
# Random package

The Random package provides support generating random numbers. The package
provides random number generators you can use in your code, a dice roller and
a trait for implementing your own random number generator.
"""
trait Random
  """
  The `Random` trait should be implemented by all random number generators. The
  only method you need to implement is `fun ref next(): 64`. Once that method
  has been implemented, the `Random` trait provides default implementations of
  conversions to other number types.
  """
  fun tag has_next(): Bool =>
    """
    If used as an iterator, this always has another value.
    """
    true

  fun ref next(): U64
    """
    A random integer in [0, 2^64)
    """

  fun ref u8(): U8 =>
    """
    A random integer in [0, 2^8)
    """
    (next() >> 56).u8()

  fun ref u16(): U16 =>
    """
    A random integer in [0, 2^16)
    """
    (next() >> 48).u16()

  fun ref u32(): U32 =>
    """
    A random integer in [0, 2^32)
    """
    (next() >> 32).u32()

  fun ref u64(): U64 =>
    """
    A random integer in [0, 2^64)
    """
    next()

  fun ref u128(): U128 =>
    """
    A random integer in [0, 2^128)
    """
    (next().u128() << 64) or next().u128()

  fun ref int(n: U64): U64 =>
    """
    A random integer in [0, n)
    """
    (real() * n.f64()).u64()

  fun ref real(): F64 =>
    """
    A random number in [0, 1)
    """
    (next() >> 11).f64() * (F64(1) / 9007199254740992)
