class ref _BitCursor
  """
  Tracks a position within an `Array[U8] val` at bit granularity.

  Ephemeral — created during a decode call and not returned in the result.
  All reads advance the cursor position.
  """
  let _data: Array[U8] val
  var _byte_offset: USize
  var _bit_offset: U8

  new ref create(
    data: Array[U8] val,
    byte_offset: USize = 0,
    bit_offset: U8 = 0)
  =>
    _data = data
    _byte_offset = byte_offset
    _bit_offset = bit_offset

  fun box position(): USize =>
    (_byte_offset * 8) + _bit_offset.usize()

  fun box available_bits(): USize =>
    if _byte_offset >= _data.size() then
      0
    else
      ((_data.size() - _byte_offset) * 8) - _bit_offset.usize()
    end

  fun box is_byte_aligned(): Bool =>
    _bit_offset == 0

  fun ref advance_bits(n: USize) =>
    let total = _bit_offset.usize() + n
    _byte_offset = _byte_offset + (total / 8)
    _bit_offset = (total % 8).u8()

  fun ref read_bits(count: U8): U64 ? =>
    """
    Read `count` bits as a big-endian unsigned integer.
    Errors if insufficient data is available.
    """
    if available_bits() < count.usize() then error end
    if count == 0 then return 0 end

    var result: U64 = 0
    var remaining = count.usize()
    var byte_pos = _byte_offset
    var bit_pos = _bit_offset

    while remaining > 0 do
      let bits_left_in_byte: USize = (8 - bit_pos.usize())
      let bits_to_read = remaining.min(bits_left_in_byte)
      let byte_val = _data(byte_pos)?
      let shift = bits_left_in_byte - bits_to_read
      let wide: U16 = (U16(1) << bits_to_read.u16()) - 1
      let mask: U8 = wide.u8() << shift.u8()
      let extracted = ((byte_val and mask) >> shift.u8()).u64()
      result = (result << bits_to_read.u64()) or extracted
      remaining = remaining - bits_to_read
      bit_pos = 0
      byte_pos = byte_pos + 1
    end

    advance_bits(count.usize())
    result

  fun ref read_bytes(count: USize): Array[U8] val ? =>
    """
    Read `count` bytes starting at the current position.
    Errors if not byte-aligned or if insufficient data is available.
    """
    if not is_byte_aligned() then error end
    if (available_bits() / 8) < count then error end
    let result = _data.trim(_byte_offset, _byte_offset + count)
    _byte_offset = _byte_offset + count
    result
