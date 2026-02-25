// TODO: maybe use a pointer here
use @lexint_double[F64](lexint: _LexIntT box)

struct _LexIntT
  """
  Compiler internal struct used for parsing numbers from literals in the source code.
  Both for floats, integers.
  For formats hexadecimal, binary and decimal
  """
  let low: U64 = 0
  let high: U64 = 0

  fun u128(): U128 =>
    low.u128() + (high.u128() << 64)

  fun u64(): U64 =>
    u128().u64()

  fun u32(): U32 =>
    u128().u32()

  fun u16(): U16 =>
    u128().u16()

  fun u8(): U8 =>
    u128().u8()

  fun usize(): USize =>
    u128().usize()

  fun ulong(): ULong =>
    u128().ulong()

  fun ref f64(): F64 =>
    @lexint_double(this)
