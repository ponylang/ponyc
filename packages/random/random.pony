"""
# Random package

The Random package provides support generating random numbers. The package
provides random number generators you can use in your code, a dice roller and
a trait for implementing your own random number generator.

If your application does not require a specific generator, use Rand.

Seed values can contain up to 128 bits of randomness in the form of two U64s.
A common non-cryptographically secure way to seed a generator is with
`Time.now`.

```pony
let rand = Rand
let n = rand.next()
```
"""
type Rand is XorOshiro128Plus

trait Random
  """
  The `Random` trait should be implemented by all random number generators. The
  only method you need to implement is `fun ref next(): U64`. Once that method
  has been implemented, the `Random` trait provides default implementations of
  conversions to other number types.
  """
  new create(x: U64 = 5489, y: U64 = 0)
    """
    Create with the specified seed. Returned values are deterministic for a
    given seed.
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

  fun ref ulong(): ULong =>
    """
    A random integer in [0, ULong.max_value()]
    """
    ifdef ilp32 or llp64 then
      (next() >> 32).ulong()
    else
      next().ulong()
    end

  fun ref usize(): USize =>
    """
    A random integer in [0, USize.max_value()]
    """
    ifdef ilp32 then
      (next() >> 32).usize()
    else
      next().usize()
    end

  fun ref i8(): I8 =>
    """
    A random integer in [-2^7, 2^7)
    """
    u8().i8()

  fun ref i16(): I16 =>
    """
    A random integer in [-2^15, 2^15)
    """
    u16().i16()

  fun ref i32(): I32 =>
    """
    A random integer in [-2^31, 2^31)
    """
    u32().i32()

  fun ref i64(): I64 =>
    """
    A random integer in [-2^63, 2^63)
    """
    u64().i64()

  fun ref i128(): I128 =>
    """
    A random integer in [-2^127, 2^127)
    """
    u128().i128()

  fun ref ilong(): ILong =>
    """
    A random integer in [ILong.min_value(), ILong.max_value()]
    """
    ulong().ilong()

  fun ref isize(): ISize =>
    """
    A random integer in [ISize.min_value(), ISize.max_value()]
    """
    usize().isize()

  fun ref int_fp_mult[N: (Unsigned val & Real[N] val) = U64](n: N): N =>
    """
    A random integer in [0, n)
    """
    N.from[F64](real() * n.f64())

  fun ref int[N: (Unsigned val & Real[N] val) = U64](n: N): N =>
    """
    A random integer in [0, n)

    Uses fixed-point inversion if platform supports native 128 bit operations
    otherwise uses floating-point multiplication.
    """
    ifdef native128 then
      // TODO: once we have specialized generic functions, chose smaller int
      // types for smaller N.
      N.from[U64](((next().u128() * n.u128()) >> 64).u64())
    else
      int_fp_mult[N](n)
    end


  fun ref int_unbiased[N: (Unsigned val & Real[N] val) = U64](n: N): N =>
    """
    A random integer in [0, n)

    Not biased with small values of `n` like `int`.
    """
    N.from[U64](_u64_unbiased(U64.from[N](n)))

  fun ref _u64_unbiased(range: U64): U64 =>
    """
    Generates a U64 in the range `[0, n)`
    while avoiding bias.

    See:
    - https://arxiv.org/abs/1805.10941
    - http://www.pcg-random.org/posts/bounded-rands.html
    """
    var x: U64 = next()
    var m: U128 = x.u128() * range.u128()
    var l: U64 = m.u64()
    if l < range then
      var t: U64 = -range
      if t >= range then
        t = t - range
        if t >= range then
          t = t % range
        end
      end
      while (l < t) do
        x = u64()
        m = x.u128() * range.u128()
        l = m.u64()
      end
    end
    (m >> 64).u64()

  fun ref real(): F64 =>
    """
    A random number in [0, 1)
    """
    (next() >> 11).f64() * (F64(1) / 9007199254740992)

  fun ref shuffle[A](array: Array[A]) =>
    """
    Shuffle the elements of the array into a random order, mutating the array.
    """
    var i: USize = array.size()
    try
      while i > 1 do
        let ceil = i = i - 1
        array.swap_elements(i, int[USize](ceil))?
      end
    end
