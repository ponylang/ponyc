primitive U8Codec is Codec[U8]
  """
  Read or write a single unsigned byte (8 bits).
  """
  fun decode(data: Array[U8] val, offset: USize)
    : ((U8, USize) | DecodeError)
  =>
    """
    Decode a single unsigned byte at the given bit offset.
    """
    let byte_offset = offset / 8
    if (offset % 8) != 0 then
      return MalformedData("U8Codec requires byte alignment", offset)
    end
    if byte_offset >= data.size() then return InsufficientData end
    try
      (data(byte_offset)?, 8)
    else
      MalformedData("Failed to read U8", offset)
    end

  fun _encode_bits(value: U8, writer: _BitWriter ref)
    : (_BitWriter ref | EncodeError)
  =>
    writer.write_bits(value.u64(), 8)
    writer

primitive I8Codec is Codec[I8]
  """
  Read or write a signed byte (8 bits).
  """
  fun decode(data: Array[U8] val, offset: USize)
    : ((I8, USize) | DecodeError)
  =>
    """
    Decode a single signed byte at the given bit offset.
    """
    let byte_offset = offset / 8
    if (offset % 8) != 0 then
      return MalformedData("I8Codec requires byte alignment", offset)
    end
    if byte_offset >= data.size() then return InsufficientData end
    try
      (data(byte_offset)?.i8(), 8)
    else
      MalformedData("Failed to read I8", offset)
    end

  fun _encode_bits(value: I8, writer: _BitWriter ref)
    : (_BitWriter ref | EncodeError)
  =>
    writer.write_bits(value.u8().u64(), 8)
    writer

primitive BoolBit is Codec[Bool]
  """
  Read or write a single bit as a `Bool`.
  """
  fun decode(data: Array[U8] val, offset: USize)
    : ((Bool, USize) | DecodeError)
  =>
    let cursor = _BitCursor(data, offset / 8, (offset % 8).u8())
    if cursor.available_bits() < 1 then return InsufficientData end
    try
      ((cursor.read_bits(1)? != 0), 1)
    else
      MalformedData("Failed to read bool bit", offset)
    end

  fun _encode_bits(value: Bool, writer: _BitWriter ref)
    : (_BitWriter ref | EncodeError)
  =>
    writer.write_bits(if value then 1 else 0 end, 1)
    writer

primitive U16BE is Codec[U16]
  """
  Read or write a 16-bit unsigned integer in big-endian byte order.
  """
  fun decode(data: Array[U8] val, offset: USize)
    : ((U16, USize) | DecodeError)
  =>
    let cursor = _BitCursor(data, offset / 8, (offset % 8).u8())
    if cursor.available_bits() < 16 then return InsufficientData end
    try
      (cursor.read_bits(16)?.u16(), 16)
    else
      MalformedData("Failed to read U16BE", offset)
    end

  fun _encode_bits(value: U16, writer: _BitWriter ref)
    : (_BitWriter ref | EncodeError)
  =>
    writer.write_bits(value.u64(), 16)
    writer

primitive U16LE is Codec[U16]
  """
  Read or write a 16-bit unsigned integer in little-endian byte order.
  """
  fun decode(data: Array[U8] val, offset: USize)
    : ((U16, USize) | DecodeError)
  =>
    """
    Decode a 16-bit unsigned integer in little-endian byte order.
    """
    if (offset % 8) != 0 then
      return MalformedData("U16LE requires byte alignment", offset)
    end
    let byte_offset = offset / 8
    if (data.size() - byte_offset) < 2 then return InsufficientData end
    try
      let lo = data(byte_offset)?.u16()
      let hi = data(byte_offset + 1)?.u16()
      ((hi << 8) or lo, 16)
    else
      MalformedData("Failed to read U16LE", offset)
    end

  fun _encode_bits(value: U16, writer: _BitWriter ref)
    : (_BitWriter ref | EncodeError)
  =>
    writer.write_byte((value and 0xFF).u8())
    writer.write_byte(((value >> 8) and 0xFF).u8())
    writer

primitive I16BE is Codec[I16]
  """
  Read or write a 16-bit signed integer in big-endian byte order.
  """
  fun decode(data: Array[U8] val, offset: USize)
    : ((I16, USize) | DecodeError)
  =>
    match \exhaustive\ U16BE.decode(data, offset)
    | (let v: U16, let c: USize) => (v.i16(), c)
    | let e: DecodeError => e
    end

  fun _encode_bits(value: I16, writer: _BitWriter ref)
    : (_BitWriter ref | EncodeError)
  =>
    U16BE._encode_bits(value.u16(), writer)

primitive I16LE is Codec[I16]
  """
  Read or write a 16-bit signed integer in little-endian byte order.
  """
  fun decode(data: Array[U8] val, offset: USize)
    : ((I16, USize) | DecodeError)
  =>
    match \exhaustive\ U16LE.decode(data, offset)
    | (let v: U16, let c: USize) => (v.i16(), c)
    | let e: DecodeError => e
    end

  fun _encode_bits(value: I16, writer: _BitWriter ref)
    : (_BitWriter ref | EncodeError)
  =>
    U16LE._encode_bits(value.u16(), writer)

primitive U32BE is Codec[U32]
  """
  Read or write a 32-bit unsigned integer in big-endian byte order.
  """
  fun decode(data: Array[U8] val, offset: USize)
    : ((U32, USize) | DecodeError)
  =>
    let cursor = _BitCursor(data, offset / 8, (offset % 8).u8())
    if cursor.available_bits() < 32 then return InsufficientData end
    try
      (cursor.read_bits(32)?.u32(), 32)
    else
      MalformedData("Failed to read U32BE", offset)
    end

  fun _encode_bits(value: U32, writer: _BitWriter ref)
    : (_BitWriter ref | EncodeError)
  =>
    writer.write_bits(value.u64(), 32)
    writer

primitive U32LE is Codec[U32]
  """
  Read or write a 32-bit unsigned integer in little-endian byte order.
  """
  fun decode(data: Array[U8] val, offset: USize)
    : ((U32, USize) | DecodeError)
  =>
    """
    Decode a 32-bit unsigned integer in little-endian byte order.
    """
    if (offset % 8) != 0 then
      return MalformedData("U32LE requires byte alignment", offset)
    end
    let byte_offset = offset / 8
    if (data.size() - byte_offset) < 4 then return InsufficientData end
    try
      let b0 = data(byte_offset)?.u32()
      let b1 = data(byte_offset + 1)?.u32()
      let b2 = data(byte_offset + 2)?.u32()
      let b3 = data(byte_offset + 3)?.u32()
      ((b3 << 24) or (b2 << 16) or (b1 << 8) or b0, 32)
    else
      MalformedData("Failed to read U32LE", offset)
    end

  fun _encode_bits(value: U32, writer: _BitWriter ref)
    : (_BitWriter ref | EncodeError)
  =>
    writer.write_byte((value and 0xFF).u8())
    writer.write_byte(((value >> 8) and 0xFF).u8())
    writer.write_byte(((value >> 16) and 0xFF).u8())
    writer.write_byte(((value >> 24) and 0xFF).u8())
    writer

primitive I32BE is Codec[I32]
  """
  Read or write a 32-bit signed integer in big-endian byte order.
  """
  fun decode(data: Array[U8] val, offset: USize)
    : ((I32, USize) | DecodeError)
  =>
    match \exhaustive\ U32BE.decode(data, offset)
    | (let v: U32, let c: USize) => (v.i32(), c)
    | let e: DecodeError => e
    end

  fun _encode_bits(value: I32, writer: _BitWriter ref)
    : (_BitWriter ref | EncodeError)
  =>
    U32BE._encode_bits(value.u32(), writer)

primitive I32LE is Codec[I32]
  """
  Read or write a 32-bit signed integer in little-endian byte order.
  """
  fun decode(data: Array[U8] val, offset: USize)
    : ((I32, USize) | DecodeError)
  =>
    match \exhaustive\ U32LE.decode(data, offset)
    | (let v: U32, let c: USize) => (v.i32(), c)
    | let e: DecodeError => e
    end

  fun _encode_bits(value: I32, writer: _BitWriter ref)
    : (_BitWriter ref | EncodeError)
  =>
    U32LE._encode_bits(value.u32(), writer)

primitive U64BE is Codec[U64]
  """
  Read or write a 64-bit unsigned integer in big-endian byte order.
  """
  fun decode(data: Array[U8] val, offset: USize)
    : ((U64, USize) | DecodeError)
  =>
    let cursor = _BitCursor(data, offset / 8, (offset % 8).u8())
    if cursor.available_bits() < 64 then return InsufficientData end
    try
      (cursor.read_bits(64)?, 64)
    else
      MalformedData("Failed to read U64BE", offset)
    end

  fun _encode_bits(value: U64, writer: _BitWriter ref)
    : (_BitWriter ref | EncodeError)
  =>
    writer.write_bits(value, 64)
    writer

primitive U64LE is Codec[U64]
  """
  Read or write a 64-bit unsigned integer in little-endian byte order.
  """
  fun decode(data: Array[U8] val, offset: USize)
    : ((U64, USize) | DecodeError)
  =>
    """
    Decode a 64-bit unsigned integer in little-endian byte order.
    """
    if (offset % 8) != 0 then
      return MalformedData("U64LE requires byte alignment", offset)
    end
    let byte_offset = offset / 8
    if (data.size() - byte_offset) < 8 then return InsufficientData end
    try
      var result: U64 = 0
      var i: USize = 0
      while i < 8 do
        result = result or (data(byte_offset + i)?.u64() << (i.u64() * 8))
        i = i + 1
      end
      (result, 64)
    else
      MalformedData("Failed to read U64LE", offset)
    end

  fun _encode_bits(value: U64, writer: _BitWriter ref)
    : (_BitWriter ref | EncodeError)
  =>
    var i: U64 = 0
    while i < 8 do
      writer.write_byte(((value >> (i * 8)) and 0xFF).u8())
      i = i + 1
    end
    writer

primitive I64BE is Codec[I64]
  """
  Read or write a 64-bit signed integer in big-endian byte order.
  """
  fun decode(data: Array[U8] val, offset: USize)
    : ((I64, USize) | DecodeError)
  =>
    match \exhaustive\ U64BE.decode(data, offset)
    | (let v: U64, let c: USize) => (v.i64(), c)
    | let e: DecodeError => e
    end

  fun _encode_bits(value: I64, writer: _BitWriter ref)
    : (_BitWriter ref | EncodeError)
  =>
    U64BE._encode_bits(value.u64(), writer)

primitive I64LE is Codec[I64]
  """
  Read or write a 64-bit signed integer in little-endian byte order.
  """
  fun decode(data: Array[U8] val, offset: USize)
    : ((I64, USize) | DecodeError)
  =>
    match \exhaustive\ U64LE.decode(data, offset)
    | (let v: U64, let c: USize) => (v.i64(), c)
    | let e: DecodeError => e
    end

  fun _encode_bits(value: I64, writer: _BitWriter ref)
    : (_BitWriter ref | EncodeError)
  =>
    U64LE._encode_bits(value.u64(), writer)

primitive F32BE is Codec[F32]
  """
  Read or write a 32-bit IEEE 754 float in big-endian byte order.
  """
  fun decode(data: Array[U8] val, offset: USize)
    : ((F32, USize) | DecodeError)
  =>
    match \exhaustive\ U32BE.decode(data, offset)
    | (let v: U32, let c: USize) => (F32.from_bits(v), c)
    | let e: DecodeError => e
    end

  fun _encode_bits(value: F32, writer: _BitWriter ref)
    : (_BitWriter ref | EncodeError)
  =>
    U32BE._encode_bits(value.bits(), writer)

primitive F32LE is Codec[F32]
  """
  Read or write a 32-bit IEEE 754 float in little-endian byte order.
  """
  fun decode(data: Array[U8] val, offset: USize)
    : ((F32, USize) | DecodeError)
  =>
    match \exhaustive\ U32LE.decode(data, offset)
    | (let v: U32, let c: USize) => (F32.from_bits(v), c)
    | let e: DecodeError => e
    end

  fun _encode_bits(value: F32, writer: _BitWriter ref)
    : (_BitWriter ref | EncodeError)
  =>
    U32LE._encode_bits(value.bits(), writer)

primitive F64BE is Codec[F64]
  """
  Read or write a 64-bit IEEE 754 float in big-endian byte order.
  """
  fun decode(data: Array[U8] val, offset: USize)
    : ((F64, USize) | DecodeError)
  =>
    match \exhaustive\ U64BE.decode(data, offset)
    | (let v: U64, let c: USize) => (F64.from_bits(v), c)
    | let e: DecodeError => e
    end

  fun _encode_bits(value: F64, writer: _BitWriter ref)
    : (_BitWriter ref | EncodeError)
  =>
    U64BE._encode_bits(value.bits(), writer)

primitive F64LE is Codec[F64]
  """
  Read or write a 64-bit IEEE 754 float in little-endian byte order.
  """
  fun decode(data: Array[U8] val, offset: USize)
    : ((F64, USize) | DecodeError)
  =>
    match \exhaustive\ U64LE.decode(data, offset)
    | (let v: U64, let c: USize) => (F64.from_bits(v), c)
    | let e: DecodeError => e
    end

  fun _encode_bits(value: F64, writer: _BitWriter ref)
    : (_BitWriter ref | EncodeError)
  =>
    U64LE._encode_bits(value.bits(), writer)

primitive U128BE is Codec[U128]
  """
  Read or write a 128-bit unsigned integer in big-endian byte order.
  """
  fun decode(data: Array[U8] val, offset: USize)
    : ((U128, USize) | DecodeError)
  =>
    """
    Decode a 128-bit unsigned integer in big-endian byte order.
    """
    if (offset % 8) != 0 then
      return MalformedData("U128BE requires byte alignment", offset)
    end
    let byte_offset = offset / 8
    if (data.size() - byte_offset) < 16 then return InsufficientData end
    try
      var result: U128 = 0
      var i: USize = 0
      while i < 16 do
        result = (result << 8) or data(byte_offset + i)?.u128()
        i = i + 1
      end
      (result, 128)
    else
      MalformedData("Failed to read U128BE", offset)
    end

  fun _encode_bits(value: U128, writer: _BitWriter ref)
    : (_BitWriter ref | EncodeError)
  =>
    writer.write_bits((value >> 64).u64(), 64)
    writer.write_bits(value.u64(), 64)
    writer

primitive U128LE is Codec[U128]
  """
  Read or write a 128-bit unsigned integer in little-endian byte order.
  """
  fun decode(data: Array[U8] val, offset: USize)
    : ((U128, USize) | DecodeError)
  =>
    """
    Decode a 128-bit unsigned integer in little-endian byte order.
    """
    if (offset % 8) != 0 then
      return MalformedData("U128LE requires byte alignment", offset)
    end
    let byte_offset = offset / 8
    if (data.size() - byte_offset) < 16 then return InsufficientData end
    try
      var result: U128 = 0
      var i: USize = 0
      while i < 16 do
        result = result or (data(byte_offset + i)?.u128() << (i.u128() * 8))
        i = i + 1
      end
      (result, 128)
    else
      MalformedData("Failed to read U128LE", offset)
    end

  fun _encode_bits(value: U128, writer: _BitWriter ref)
    : (_BitWriter ref | EncodeError)
  =>
    var i: U128 = 0
    while i < 16 do
      writer.write_byte(((value >> (i * 8)) and 0xFF).u8())
      i = i + 1
    end
    writer

primitive I128BE is Codec[I128]
  """
  Read or write a 128-bit signed integer in big-endian byte order.
  """
  fun decode(data: Array[U8] val, offset: USize)
    : ((I128, USize) | DecodeError)
  =>
    match \exhaustive\ U128BE.decode(data, offset)
    | (let v: U128, let c: USize) => (v.i128(), c)
    | let e: DecodeError => e
    end

  fun _encode_bits(value: I128, writer: _BitWriter ref)
    : (_BitWriter ref | EncodeError)
  =>
    U128BE._encode_bits(value.u128(), writer)

primitive I128LE is Codec[I128]
  """
  Read or write a 128-bit signed integer in little-endian byte order.
  """
  fun decode(data: Array[U8] val, offset: USize)
    : ((I128, USize) | DecodeError)
  =>
    match \exhaustive\ U128LE.decode(data, offset)
    | (let v: U128, let c: USize) => (v.i128(), c)
    | let e: DecodeError => e
    end

  fun _encode_bits(value: I128, writer: _BitWriter ref)
    : (_BitWriter ref | EncodeError)
  =>
    U128LE._encode_bits(value.u128(), writer)
