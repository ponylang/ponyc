class val BitsU8 is Codec[U8]
  """
  Read or write an unsigned integer of a specified bit width (1–8 bits),
  returning a `U8`.
  """
  let _count: U8

  new val create(count: U8) =>
    _count = count

  fun decode(data: Array[U8] val, offset: USize)
    : ((U8, USize) | DecodeError)
  =>
    let cursor = _BitCursor(data, offset / 8, (offset % 8).u8())
    if cursor.available_bits() < _count.usize() then
      return InsufficientData
    end
    try
      (cursor.read_bits(_count)?.u8(), _count.usize())
    else
      MalformedData("Failed to read " + _count.string() + " bits", offset)
    end

  fun _encode_bits(value: U8, writer: _BitWriter ref)
    : (_BitWriter ref | EncodeError)
  =>
    if (_count < 8) and ((value >> _count) > 0) then
      return EncodeViolation(
        value.string() + " exceeds " + _count.string() + " bits")
    end
    writer.write_bits(value.u64(), _count)
    writer

class val BitsU16 is Codec[U16]
  """
  Read or write an unsigned integer of a specified bit width (1–16 bits),
  returning a `U16`. Bits are read big-endian.
  """
  let _count: U8

  new val create(count: U8) =>
    _count = count

  fun decode(data: Array[U8] val, offset: USize)
    : ((U16, USize) | DecodeError)
  =>
    let cursor = _BitCursor(data, offset / 8, (offset % 8).u8())
    if cursor.available_bits() < _count.usize() then
      return InsufficientData
    end
    try
      (cursor.read_bits(_count)?.u16(), _count.usize())
    else
      MalformedData("Failed to read " + _count.string() + " bits", offset)
    end

  fun _encode_bits(value: U16, writer: _BitWriter ref)
    : (_BitWriter ref | EncodeError)
  =>
    if (_count < 16) and ((value >> _count.u16()) > 0) then
      return EncodeViolation(
        value.string() + " exceeds " + _count.string() + " bits")
    end
    writer.write_bits(value.u64(), _count)
    writer

class val BitsU32 is Codec[U32]
  """
  Read or write an unsigned integer of a specified bit width (1–32 bits),
  returning a `U32`. Bits are read big-endian.
  """
  let _count: U8

  new val create(count: U8) =>
    _count = count

  fun decode(data: Array[U8] val, offset: USize)
    : ((U32, USize) | DecodeError)
  =>
    let cursor = _BitCursor(data, offset / 8, (offset % 8).u8())
    if cursor.available_bits() < _count.usize() then
      return InsufficientData
    end
    try
      (cursor.read_bits(_count)?.u32(), _count.usize())
    else
      MalformedData("Failed to read " + _count.string() + " bits", offset)
    end

  fun _encode_bits(value: U32, writer: _BitWriter ref)
    : (_BitWriter ref | EncodeError)
  =>
    if (_count < 32) and ((value >> _count.u32()) > 0) then
      return EncodeViolation(
        value.string() + " exceeds " + _count.string() + " bits")
    end
    writer.write_bits(value.u64(), _count)
    writer

class val BitsU64 is Codec[U64]
  """
  Read or write an unsigned integer of a specified bit width (1–64 bits),
  returning a `U64`. Bits are read big-endian.
  """
  let _count: U8

  new val create(count: U8) =>
    _count = count

  fun decode(data: Array[U8] val, offset: USize)
    : ((U64, USize) | DecodeError)
  =>
    let cursor = _BitCursor(data, offset / 8, (offset % 8).u8())
    if cursor.available_bits() < _count.usize() then
      return InsufficientData
    end
    try
      (cursor.read_bits(_count)?, _count.usize())
    else
      MalformedData("Failed to read " + _count.string() + " bits", offset)
    end

  fun _encode_bits(value: U64, writer: _BitWriter ref)
    : (_BitWriter ref | EncodeError)
  =>
    if (_count < 64) and ((value >> _count.u64()) > 0) then
      return EncodeViolation(
        value.string() + " exceeds " + _count.string() + " bits")
    end
    writer.write_bits(value, _count)
    writer
