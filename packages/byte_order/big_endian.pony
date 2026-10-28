primitive BigEndian
  """
  Read and write multi-byte values in big-endian (network) byte order.

  Read methods are generic over any `ReadSeq[U8]`. When the concrete
  sequence is an `Array[U8]`, an optimized path uses a single native-width
  load and a conditional byte swap.
  """

  fun u16[S: ReadSeq[U8] box](seq: S, offset: USize = 0): U16 ? =>
    """
    Read a big-endian U16 from `seq` at `offset`.
    """
    (seq(offset)?.u16() << 8) or seq(offset + 1)?.u16()

  fun u16[S: ReadSeq[U8] box](seq: S, offset: USize = 0): U16
    iftype S <: Array[U8] box
  ? =>
    ifdef bigendian then
      seq.read_u16(offset)?
    else
      seq.read_u16(offset)?.bswap()
    end

  fun u32[S: ReadSeq[U8] box](seq: S, offset: USize = 0): U32 ? =>
    """
    Read a big-endian U32 from `seq` at `offset`.
    """
    (u16[S](seq, offset)?.u32() << 16) or u16[S](seq, offset + 2)?.u32()

  fun u32[S: ReadSeq[U8] box](seq: S, offset: USize = 0): U32
    iftype S <: Array[U8] box
  ? =>
    ifdef bigendian then
      seq.read_u32(offset)?
    else
      seq.read_u32(offset)?.bswap()
    end

  fun u64[S: ReadSeq[U8] box](seq: S, offset: USize = 0): U64 ? =>
    """
    Read a big-endian U64 from `seq` at `offset`.
    """
    (u32[S](seq, offset)?.u64() << 32) or u32[S](seq, offset + 4)?.u64()

  fun u64[S: ReadSeq[U8] box](seq: S, offset: USize = 0): U64
    iftype S <: Array[U8] box
  ? =>
    ifdef bigendian then
      seq.read_u64(offset)?
    else
      seq.read_u64(offset)?.bswap()
    end

  fun u128[S: ReadSeq[U8] box](seq: S, offset: USize = 0): U128 ? =>
    """
    Read a big-endian U128 from `seq` at `offset`.
    """
    (u64[S](seq, offset)?.u128() << 64) or
      u64[S](seq, offset + 8)?.u128()

  fun u128[S: ReadSeq[U8] box](seq: S, offset: USize = 0): U128
    iftype S <: Array[U8] box
  ? =>
    ifdef bigendian then
      seq.read_u128(offset)?
    else
      seq.read_u128(offset)?.bswap()
    end

  fun i16[S: ReadSeq[U8] box](seq: S, offset: USize = 0): I16 ? =>
    """
    Read a big-endian I16 from `seq` at `offset`.
    """
    u16[S](seq, offset)?.i16()

  fun i32[S: ReadSeq[U8] box](seq: S, offset: USize = 0): I32 ? =>
    """
    Read a big-endian I32 from `seq` at `offset`.
    """
    u32[S](seq, offset)?.i32()

  fun i64[S: ReadSeq[U8] box](seq: S, offset: USize = 0): I64 ? =>
    """
    Read a big-endian I64 from `seq` at `offset`.
    """
    u64[S](seq, offset)?.i64()

  fun i128[S: ReadSeq[U8] box](seq: S, offset: USize = 0): I128 ? =>
    """
    Read a big-endian I128 from `seq` at `offset`.
    """
    u128[S](seq, offset)?.i128()

  fun f32[S: ReadSeq[U8] box](seq: S, offset: USize = 0): F32 ? =>
    """
    Read a big-endian F32 from `seq` at `offset`.
    """
    F32.from_bits(u32[S](seq, offset)?)

  fun f64[S: ReadSeq[U8] box](seq: S, offset: USize = 0): F64 ? =>
    """
    Read a big-endian F64 from `seq` at `offset`.
    """
    F64.from_bits(u64[S](seq, offset)?)

  fun push_u16(into: Array[U8] ref, value: U16) =>
    """
    Append a U16 to `into` in big-endian byte order.
    """
    into.push((value >> 8).u8())
    into.push(value.u8())

  fun push_u32(into: Array[U8] ref, value: U32) =>
    """
    Append a U32 to `into` in big-endian byte order.
    """
    push_u16(into, (value >> 16).u16())
    push_u16(into, value.u16())

  fun push_u64(into: Array[U8] ref, value: U64) =>
    """
    Append a U64 to `into` in big-endian byte order.
    """
    push_u32(into, (value >> 32).u32())
    push_u32(into, value.u32())

  fun push_u128(into: Array[U8] ref, value: U128) =>
    """
    Append a U128 to `into` in big-endian byte order.
    """
    push_u64(into, (value >> 64).u64())
    push_u64(into, value.u64())

  fun push_i16(into: Array[U8] ref, value: I16) =>
    """
    Append an I16 to `into` in big-endian byte order.
    """
    push_u16(into, value.u16())

  fun push_i32(into: Array[U8] ref, value: I32) =>
    """
    Append an I32 to `into` in big-endian byte order.
    """
    push_u32(into, value.u32())

  fun push_i64(into: Array[U8] ref, value: I64) =>
    """
    Append an I64 to `into` in big-endian byte order.
    """
    push_u64(into, value.u64())

  fun push_i128(into: Array[U8] ref, value: I128) =>
    """
    Append an I128 to `into` in big-endian byte order.
    """
    push_u128(into, value.u128())

  fun push_f32(into: Array[U8] ref, value: F32) =>
    """
    Append an F32 to `into` in big-endian byte order.
    """
    push_u32(into, value.bits())

  fun push_f64(into: Array[U8] ref, value: F64) =>
    """
    Append an F64 to `into` in big-endian byte order.
    """
    push_u64(into, value.bits())
