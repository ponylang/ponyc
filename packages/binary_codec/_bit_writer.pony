class ref _BitWriter
  """
  Accumulates bits for encoding, producing a byte array when complete.

  Bits are written big-endian within each byte (MSB first). The output
  must be byte-aligned when `done()` is called.
  """
  var _buffer: Array[U8] ref
  var _current_byte: U8 = 0
  var _bit_pos: U8 = 0

  new ref create(initial_size: USize = 64) =>
    _buffer = Array[U8](initial_size)

  fun ref write_bits(value: U64, count: U8) =>
    """
    Write `count` bits from `value`, MSB first.
    """
    if count == 0 then return end

    var remaining = count.usize()
    var val_bits = value

    while remaining > 0 do
      let space = (8 - _bit_pos.usize())
      let bits_to_write = remaining.min(space)
      let shift = remaining - bits_to_write
      let extracted =
        ((val_bits >> shift.u64()) and ((U64(1) << bits_to_write.u64()) - 1))
          .u8()
      let positioned = extracted << (space - bits_to_write).u8()
      _current_byte = _current_byte or positioned
      _bit_pos = _bit_pos + bits_to_write.u8()
      remaining = remaining - bits_to_write
      val_bits = val_bits and ((U64(1) << shift.u64()) - 1)

      if _bit_pos == 8 then
        _buffer.push(_current_byte)
        _current_byte = 0
        _bit_pos = 0
      end
    end

  fun ref write_bytes(data: Array[U8] box) =>
    """
    Write whole bytes. Must be byte-aligned.
    """
    if _bit_pos == 0 then
      for b in data.values() do
        _buffer.push(b)
      end
    end

  fun ref write_byte(value: U8) =>
    if _bit_pos == 0 then
      _buffer.push(value)
    end

  fun ref done(): Array[U8] val ? =>
    """
    Finalize and return the accumulated bytes.
    Errors if the output is not byte-aligned.
    """
    if _bit_pos != 0 then error end
    let result: Array[U8] val = _buffer.clone()
    _buffer = Array[U8](64)
    _current_byte = 0
    _bit_pos = 0
    result
