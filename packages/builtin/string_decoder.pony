trait val StringDecoder
  """
  A Decoder converts bytes into unicode codepoints.
  """
  new val create()

  fun decode(b:U32): (U32, U8)
  """
  Convert up to 4 bytes packed in a U32 into a unicode codepoint. Return a pair
  containing the codepoint (U32) and the number of bytes consumed. Bytes are
  consumed starting with the most significant bits in the input U32. If the bytes
  cannot be converted to a codepoint, codepoint 0xFFFD is returned.
  """

class StringDecoderBytes
  """
  A class that maintains a U32 that can be loaded with bytes from a byte stream and
  passed to the decode function.
  """
  var _decode_bytes: U32 = 0
  var _bytes_loaded: U8 = 0

  fun ref pushByte(b: U8) =>
    if _bytes_loaded == 0 then
      _decode_bytes = (_decode_bytes or (b.u32() << 24))
    elseif _bytes_loaded == 1 then
      _decode_bytes = (_decode_bytes or (b.u32() << 16))
    elseif _bytes_loaded == 2 then
      _decode_bytes = (_decode_bytes or (b.u32() << 8))
    elseif _bytes_loaded == 3 then
      _decode_bytes = _decode_bytes or b.u32()
    else
      return
    end
    _bytes_loaded = _bytes_loaded + 1

  fun bytes_loaded(): U8 =>
    _bytes_loaded

  fun decode_bytes(): U32 =>
    _decode_bytes

  fun ref process_bytes(count: U8) =>
    if (count == 4) then
      _decode_bytes = 0
    else
      _decode_bytes = (_decode_bytes <<~ (count * 8).u32())
    end
    _bytes_loaded = _bytes_loaded - count
