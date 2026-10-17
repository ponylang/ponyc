class val Bytes is Codec[Array[U8] val]
  """
  Read or write exactly `count` bytes.

  Requires byte alignment. On encode, the array must have exactly `count`
  elements.
  """
  let _count: USize

  new val create(count: USize) =>
    _count = count

  fun decode(data: Array[U8] val, offset: USize)
    : ((Array[U8] val, USize) | DecodeError)
  =>
    """
    Decode `count` bytes starting at the given bit offset.
    """
    if (offset % 8) != 0 then
      return MalformedData("Bytes requires byte alignment", offset)
    end
    let byte_offset = offset / 8
    if (data.size() - byte_offset) < _count then
      return InsufficientData
    end
    try
      let cursor = _BitCursor(data, byte_offset)
      (cursor.read_bytes(_count)?, _count * 8)
    else
      MalformedData("Failed to read bytes", offset)
    end

  fun _encode_bits(value: Array[U8] val, writer: _BitWriter ref)
    : (_BitWriter ref | EncodeError)
  =>
    if value.size() != _count then
      return EncodeViolation(
        "Expected " + _count.string() + " bytes, got " +
          value.size().string())
    end
    writer.write_bytes(value)
    writer

primitive BytesRest is Codec[Array[U8] val]
  """
  Read all remaining bytes, or write an array of bytes with no length
  constraint.

  Requires byte alignment. On decode, consumes everything from the current
  position to the end of the input.
  """
  fun decode(data: Array[U8] val, offset: USize)
    : ((Array[U8] val, USize) | DecodeError)
  =>
    """
    Decode all bytes from the current position to the end of the input.
    """
    if (offset % 8) != 0 then
      return MalformedData("BytesRest requires byte alignment", offset)
    end
    let byte_offset = offset / 8
    let remaining = data.size() - byte_offset
    try
      let cursor = _BitCursor(data, byte_offset)
      (cursor.read_bytes(remaining)?, remaining * 8)
    else
      MalformedData("Failed to read remaining bytes", offset)
    end

  fun _encode_bits(value: Array[U8] val, writer: _BitWriter ref)
    : (_BitWriter ref | EncodeError)
  =>
    writer.write_bytes(value)
    writer

class val Literal is Codec[None]
  """
  Match or emit a fixed byte sequence.

  On decode, the bytes at the current position must match exactly or
  `MalformedData` is returned. On encode, the fixed bytes are emitted.
  The decoded value is `None` — literals carry no data.
  """
  let _expected: Array[U8] val

  new val create(expected: Array[U8] val) =>
    _expected = expected

  fun decode(data: Array[U8] val, offset: USize)
    : ((None, USize) | DecodeError)
  =>
    """
    Decode the expected byte sequence, returning `None` on match.
    """
    if (offset % 8) != 0 then
      return MalformedData("Literal requires byte alignment", offset)
    end
    let byte_offset = offset / 8
    if (data.size() - byte_offset) < _expected.size() then
      return InsufficientData
    end
    try
      var i: USize = 0
      while i < _expected.size() do
        if data(byte_offset + i)? != _expected(i)? then
          return MalformedData("Literal mismatch", offset + (i * 8))
        end
        i = i + 1
      end
      (None, _expected.size() * 8)
    else
      MalformedData("Failed to read literal", offset)
    end

  fun _encode_bits(value: None, writer: _BitWriter ref)
    : (_BitWriter ref | EncodeError)
  =>
    writer.write_bytes(_expected)
    writer
