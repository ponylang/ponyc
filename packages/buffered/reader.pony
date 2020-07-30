use "collections"

class Reader
  """
  Store network data and provide a parsing interface.

  `Reader` provides a way to extract typed data from a sequence of
  bytes. The `Reader` manages the underlying data structures to
  provide a read cursor over a contiguous sequence of bytes. It is
  useful for decoding data that is received over a network or stored
  in a file. Chunk of bytes are added to the `Reader` using the
  `append` method, and typed data is extracted using the getter
  methods.

  For example, suppose we have a UDP-based network data protocol where
  messages consist of the following:

  * `list_size` - the number of items in the following list of items
    as a big-endian 32-bit integer
  * zero or more items of the following data:
    * a big-endian 64-bit floating point number
    * a string that starts with a big-endian 32-bit integer that
      specifies the length of the string, followed by a number of
      bytes that represent the string

  A message would be something like this:

  ```
  [message_length][list_size][float1][string1][float2][string2]...
  ```

  The following program uses a `Reader` to decode a message of
  this type and print them:

  ```pony
  use "buffered"
  use "collections"

  class Notify is InputNotify
    let _env: Env
    new create(env: Env) =>
      _env = env
    fun ref apply(data: Array[U8] iso) =>
      let rb = Reader
      rb.append(consume data)
      try
        while true do
          let len = rb.i32_be()?
          let items = rb.i32_be()?.usize()
          for range in Range(0, items) do
            let f = rb.f32_be()?
            let str_len = rb.i32_be()?.usize()
            let str = String.from_array(rb.block(str_len)?)
            _env.out.print("[(" + f.string() + "), (" + str + ")]")
          end
        end
      end

  actor Main
    new create(env: Env) =>
      env.input(recover Notify(env) end, 1024)
  ```
  """
  embed _chunks: List[(Array[U8] val, USize)] = _chunks.create()
  var _available: USize = 0

  fun size(): USize =>
    """
    Return the number of available bytes.
    """
    _available

  fun ref clear() =>
    """
    Discard all pending data.
    """
    _chunks.clear()
    _available = 0

  fun ref append(data: ByteSeq) =>
    """
    Add a chunk of data.
    """
    _available = _available + data.size()
    _chunks.push((data, 0))

  fun ref skip(n: USize) ? =>
    """
    Skip n bytes.
    """
    if _available >= n then
      _available = _available - n
      var rem = n

      while rem > 0 do
        let node = _chunks.head()?
        (var data, var offset) = node()?
        let avail = data.size() - offset

        if avail > rem then
          node()? = (data, offset + rem)
          break
        end

        rem = rem - avail
        _chunks.shift()?
      end

    else
      error
    end

  fun ref block(len: USize): Array[U8] iso^ ? =>
    """
    Return a block as a contiguous chunk of memory.
    Will throw an error if you request a block larger than what is currently
    stored in the `Reader`.
    """
    if _available < len then
      error
    end

    _available = _available - len
    var out = recover Array[U8] .> undefined(len) end
    var i = USize(0)

    while i < len do
      let node = _chunks.head()?
      (let data, let offset) = node()?

      let avail = data.size() - offset
      let need = len - i
      let copy_len = need.min(avail)

      out = recover
        let r = consume ref out
        data.copy_to(r, offset, i, copy_len)
        consume r
      end

      if avail > need then
        node()? = (data, offset + need)
        break
      end

      i = i + copy_len
      _chunks.shift()?
    end

    out


  fun ref read_until(separator: U8): Array[U8] iso^ ? =>
    """
    Find the first occurrence of the separator and return the block of bytes
    before its position. The separator is not included in the returned array,
    but it is removed from the buffer. To read a line of text, prefer line()
    that handles \n and \r\n.
    """
    let b = block(_distance_of(separator)? - 1)?
    u8()?
    b

  fun ref codepoint[D: StringDecoder = UTF8StringDecoder](): (U32, U8) ? =>
    """
    Return a pair containing a unicode codepoint, and the number of bytes consumed to produce
    the codepoint. Depending on how bytes are decoded into characters, the number of bytes consumed
    may be greater than one. If the bytes cannot be converted to a codepoint, codepoint 0xFFFD
    is returned, and 1 byte is consumed.
    """
    let decoder_bytes = StringDecoderBytes.create()
    while (decoder_bytes.bytes_loaded() < 4) do
      try
        decoder_bytes.pushByte(peek_u8(decoder_bytes.bytes_loaded().usize())?)
      else
        if decoder_bytes.bytes_loaded() > 0 then
          (let c, let sz) = D.decode(decoder_bytes.decode_bytes())
          block(sz.usize())? // We ignore the bytes returned, but this will mark the bytes decoded into a character as consumed
          return (c, sz)
        else
          error
        end
      end
    end

    try
      (let c, let sz) = D.decode(decoder_bytes.decode_bytes())
      block(sz.usize())? // We ignore the bytes returned, but this will mark the bytes decoded into a character as consumed
      return (c, sz)
    end
    (0,0) // This should never happen

  fun ref string[D: StringDecoder = UTF8StringDecoder](len: USize): (String iso^, USize) ? =>
    """
    Return a pair containing a string of the specified length in characters, and the number of bytes consumed
    to produce the string. Depending on how bytes are decoded into characters, the number of bytes consumed
    may be greater than the number of characters in the string. Invalid byte sequences may result in 0xFFFD
    codepoints appearing in the string.
    """
    var chars_read: USize = 0
    var bytes_read: USize = 0
    var result: String iso = recover String(len) end
    while (chars_read < len) do
      (let c, let sz) = codepoint[D]()?
      result.push(c)
      chars_read = chars_read + 1
      bytes_read = bytes_read + sz.usize()
    end
    (consume result, bytes_read)

  fun ref line[D: StringDecoder = UTF8StringDecoder](keep_line_breaks: Bool = false): (String iso^, USize) ? =>
    """
    Return a pair containing a \n or \r\n terminated line as a string, and the number
    of bytes consumed to produce the string.  By default the newline is not
    included in the returned string, but it is removed from the buffer.
    Set `keep_line_breaks` to `true` to keep the line breaks in the returned line.
    """
    let len = _search_length()?

    _available = _available - len
    var outb = recover Array[U8](len) end
    var i = USize(0)

    while i < len do
      let node = _chunks.head()?
      (let data, let offset) = node()?

      let avail = data.size() - offset
      let need = len - i
      let copy_len = need.min(avail)

      outb.append(data, offset, copy_len)

      if avail > need then
        node()? = (data, offset + need)
        break
      end

      i = i + copy_len
      _chunks.shift()?
    end

    let trunc_len: USize =
      if keep_line_breaks then
        0
      elseif (len >= 2) and (outb.apply(outb.size()-2)? == '\r') then
        2
      else
        1
      end
    outb.truncate(len - trunc_len)

    var out = recover String.from_iso_array[D](consume outb) end

    (consume out, len)

  fun ref u8(): U8 ? =>
    """
    Get a U8. Raise an error if there isn't enough data.
    """
    if _available >= 1 then
      _byte()?
    else
      error
    end

  fun ref i8(): I8 ? =>
    """
    Get an I8.
    """
    u8()?.i8()

  fun ref u16_be(): U16 ? =>
    """
    Get a big-endian U16.
    """
    let num_bytes = U16(0).bytewidth()
    if _available >= num_bytes then
      let node = _chunks.head()?
      (var data, var offset) = node()?
      if (data.size() - offset) >= num_bytes then
        let r =
          ifdef bigendian then
            data.read_u16(offset)?
          else
            data.read_u16(offset)?.bswap()
          end

        offset = offset + num_bytes
        _available = _available - num_bytes

        if offset < data.size() then
          node()? = (data, offset)
        else
          _chunks.shift()?
        end
        r
      else
        // single array did not have all the bytes needed
        (u8()?.u16() << 8) or u8()?.u16()
      end
    else
      error
    end

  fun ref u16_le(): U16 ? =>
    """
    Get a little-endian U16.
    """
    let num_bytes = U16(0).bytewidth()
    if _available >= num_bytes then
      let node = _chunks.head()?
      (var data, var offset) = node()?
      if (data.size() - offset) >= num_bytes then
        let r =
          ifdef littleendian then
            data.read_u16(offset)?
          else
            data.read_u16(offset)?.bswap()
          end

        offset = offset + num_bytes
        _available = _available - num_bytes

        if offset < data.size() then
          node()? = (data, offset)
        else
          _chunks.shift()?
        end
        r
      else
        // single array did not have all the bytes needed
        u8()?.u16() or (u8()?.u16() << 8)
      end
    else
      error
    end

  fun ref i16_be(): I16 ? =>
    """
    Get a big-endian I16.
    """
    u16_be()?.i16()

  fun ref i16_le(): I16 ? =>
    """
    Get a little-endian I16.
    """
    u16_le()?.i16()

  fun ref u32_be(): U32 ? =>
    """
    Get a big-endian U32.
    """
    let num_bytes = U32(0).bytewidth()
    if _available >= num_bytes then
      let node = _chunks.head()?
      (var data, var offset) = node()?
      if (data.size() - offset) >= num_bytes then
        let r =
          ifdef bigendian then
            data.read_u32(offset)?
          else
            data.read_u32(offset)?.bswap()
          end

        offset = offset + num_bytes
        _available = _available - num_bytes

        if offset < data.size() then
          node()? = (data, offset)
        else
          _chunks.shift()?
        end
        r
      else
        // single array did not have all the bytes needed
        (u8()?.u32() << 24) or (u8()?.u32() << 16) or
          (u8()?.u32() << 8) or u8()?.u32()
      end
    else
      error
    end

  fun ref u32_le(): U32 ? =>
    """
    Get a little-endian U32.
    """
    let num_bytes = U32(0).bytewidth()
    if _available >= num_bytes then
      let node = _chunks.head()?
      (var data, var offset) = node()?
      if (data.size() - offset) >= num_bytes then
        let r =
          ifdef littleendian then
            data.read_u32(offset)?
          else
            data.read_u32(offset)?.bswap()
          end

        offset = offset + num_bytes
        _available = _available - num_bytes

        if offset < data.size() then
          node()? = (data, offset)
        else
          _chunks.shift()?
        end
        r
      else
        // single array did not have all the bytes needed
        u8()?.u32() or (u8()?.u32() << 8) or
          (u8()?.u32() << 16) or (u8()?.u32() << 24)
      end
    else
      error
    end

  fun ref i32_be(): I32 ? =>
    """
    Get a big-endian I32.
    """
    u32_be()?.i32()

  fun ref i32_le(): I32 ? =>
    """
    Get a little-endian I32.
    """
    u32_le()?.i32()

  fun ref u64_be(): U64 ? =>
    """
    Get a big-endian U64.
    """
    let num_bytes = U64(0).bytewidth()
    if _available >= num_bytes then
      let node = _chunks.head()?
      (var data, var offset) = node()?
      if (data.size() - offset) >= num_bytes then
        let r =
          ifdef bigendian then
            data.read_u64(offset)?
          else
            data.read_u64(offset)?.bswap()
          end

        offset = offset + num_bytes
        _available = _available - num_bytes

        if offset < data.size() then
          node()? = (data, offset)
        else
          _chunks.shift()?
        end
        r
      else
        // single array did not have all the bytes needed
        (u8()?.u64() << 56) or (u8()?.u64() << 48) or
          (u8()?.u64() << 40) or (u8()?.u64() << 32) or
          (u8()?.u64() << 24) or (u8()?.u64() << 16) or
          (u8()?.u64() << 8) or u8()?.u64()
      end
    else
      error
    end

  fun ref u64_le(): U64 ? =>
    """
    Get a little-endian U64.
    """
    let num_bytes = U64(0).bytewidth()
    if _available >= num_bytes then
      let node = _chunks.head()?
      (var data, var offset) = node()?
      if (data.size() - offset) >= num_bytes then
        let r =
          ifdef littleendian then
            data.read_u64(offset)?
          else
            data.read_u64(offset)?.bswap()
          end

        offset = offset + num_bytes
        _available = _available - num_bytes

        if offset < data.size() then
          node()? = (data, offset)
        else
          _chunks.shift()?
        end
        r
      else
        // single array did not have all the bytes needed
        u8()?.u64() or (u8()?.u64() << 8) or
          (u8()?.u64() << 16) or (u8()?.u64() << 24) or
          (u8()?.u64() << 32) or (u8()?.u64() << 40) or
          (u8()?.u64() << 48) or (u8()?.u64() << 56)
      end
    else
      error
    end

  fun ref i64_be(): I64 ? =>
    """
    Get a big-endian I64.
    """
    u64_be()?.i64()

  fun ref i64_le(): I64 ? =>
    """
    Get a little-endian I64.
    """
    u64_le()?.i64()

  fun ref u128_be(): U128 ? =>
    """
    Get a big-endian U128.
    """
    let num_bytes = U128(0).bytewidth()
    if _available >= num_bytes then
      let node = _chunks.head()?
      (var data, var offset) = node()?
      if (data.size() - offset) >= num_bytes then
        let r =
          ifdef bigendian then
            data.read_u128(offset)?
          else
            data.read_u128(offset)?.bswap()
          end

        offset = offset + num_bytes
        _available = _available - num_bytes

        if offset < data.size() then
          node()? = (data, offset)
        else
          _chunks.shift()?
        end
        r
      else
        // single array did not have all the bytes needed
        (u8()?.u128() << 120) or (u8()?.u128() << 112) or
          (u8()?.u128() << 104) or (u8()?.u128() << 96) or
          (u8()?.u128() << 88) or (u8()?.u128() << 80) or
          (u8()?.u128() << 72) or (u8()?.u128() << 64) or
          (u8()?.u128() << 56) or (u8()?.u128() << 48) or
          (u8()?.u128() << 40) or (u8()?.u128() << 32) or
          (u8()?.u128() << 24) or (u8()?.u128() << 16) or
          (u8()?.u128() << 8) or u8()?.u128()
      end
    else
      error
    end

  fun ref u128_le(): U128 ? =>
    """
    Get a little-endian U128.
    """
    let num_bytes = U128(0).bytewidth()
    if _available >= num_bytes then
      let node = _chunks.head()?
      (var data, var offset) = node()?
      if (data.size() - offset) >= num_bytes then
        let r =
          ifdef littleendian then
            data.read_u128(offset)?
          else
            data.read_u128(offset)?.bswap()
          end

        offset = offset + num_bytes
        _available = _available - num_bytes

        if offset < data.size() then
          node()? = (data, offset)
        else
          _chunks.shift()?
        end
        r
      else
        // single array did not have all the bytes needed
        u8()?.u128() or (u8()?.u128() << 8) or
          (u8()?.u128() << 16) or (u8()?.u128() << 24) or
          (u8()?.u128() << 32) or (u8()?.u128() << 40) or
          (u8()?.u128() << 48) or (u8()?.u128() << 56) or
          (u8()?.u128() << 64) or (u8()?.u128() << 72) or
          (u8()?.u128() << 80) or (u8()?.u128() << 88) or
          (u8()?.u128() << 96) or (u8()?.u128() << 104) or
          (u8()?.u128() << 112) or (u8()?.u128() << 120)
      end
    else
      error
    end

  fun ref i128_be(): I128 ? =>
    """
    Get a big-endian I129.
    """
    u128_be()?.i128()

  fun ref i128_le(): I128 ? =>
    """
    Get a little-endian I128.
    """
    u128_le()?.i128()

  fun ref f32_be(): F32 ? =>
    """
    Get a big-endian F32.
    """
    F32.from_bits(u32_be()?)

  fun ref f32_le(): F32 ? =>
    """
    Get a little-endian F32.
    """
    F32.from_bits(u32_le()?)

  fun ref f64_be(): F64 ? =>
    """
    Get a big-endian F64.
    """
    F64.from_bits(u64_be()?)

  fun ref f64_le(): F64 ? =>
    """
    Get a little-endian F64.
    """
    F64.from_bits(u64_le()?)

  fun ref _byte(): U8 ? =>
    """
    Get a single byte.
    """
    let node = _chunks.head()?
    (var data, var offset) = node()?
    let r = data(offset)?

    offset = offset + 1
    _available = _available - 1

    if offset < data.size() then
      node()? = (data, offset)
    else
      _chunks.shift()?
    end
    r

  fun box peek_u8(offset: USize = 0): U8 ? =>
    """
    Peek at a U8 at the given offset. Raise an error if there isn't enough
    data.
    """
    _peek_byte(offset)?

  fun box peek_i8(offset: USize = 0): I8 ? =>
    """
    Peek at an I8.
    """
    peek_u8(offset)?.i8()

  fun box peek_u16_be(offset: USize = 0): U16 ? =>
    """
    Peek at a big-endian U16.
    """
    (peek_u8(offset)?.u16() << 8) or peek_u8(offset + 1)?.u16()

  fun box peek_u16_le(offset: USize = 0): U16 ? =>
    """
    Peek at a little-endian U16.
    """
    peek_u8(offset)?.u16() or (peek_u8(offset + 1)?.u16() << 8)

  fun box peek_i16_be(offset: USize = 0): I16 ? =>
    """
    Peek at a big-endian I16.
    """
    peek_u16_be(offset)?.i16()

  fun box peek_i16_le(offset: USize = 0): I16 ? =>
    """
    Peek at a little-endian I16.
    """
    peek_u16_le(offset)?.i16()

  fun box peek_u32_be(offset: USize = 0): U32 ? =>
    """
    Peek at a big-endian U32.
    """
    (peek_u16_be(offset)?.u32() << 16) or peek_u16_be(offset + 2)?.u32()

  fun box peek_u32_le(offset: USize = 0): U32 ? =>
    """
    Peek at a little-endian U32.
    """
    peek_u16_le(offset)?.u32() or (peek_u16_le(offset + 2)?.u32() << 16)

  fun box peek_i32_be(offset: USize = 0): I32 ? =>
    """
    Peek at a big-endian I32.
    """
    peek_u32_be(offset)?.i32()

  fun box peek_i32_le(offset: USize = 0): I32 ? =>
    """
    Peek at a little-endian I32.
    """
    peek_u32_le(offset)?.i32()

  fun box peek_u64_be(offset: USize = 0): U64 ? =>
    """
    Peek at a big-endian U64.
    """
    (peek_u32_be(offset)?.u64() << 32) or peek_u32_be(offset + 4)?.u64()

  fun box peek_u64_le(offset: USize = 0): U64 ? =>
    """
    Peek at a little-endian U64.
    """
    peek_u32_le(offset)?.u64() or (peek_u32_le(offset + 4)?.u64() << 32)

  fun box peek_i64_be(offset: USize = 0): I64 ? =>
    """
    Peek at a big-endian I64.
    """
    peek_u64_be(offset)?.i64()

  fun box peek_i64_le(offset: USize = 0): I64 ? =>
    """
    Peek at a little-endian I64.
    """
    peek_u64_le(offset)?.i64()

  fun box peek_u128_be(offset: USize = 0): U128 ? =>
    """
    Peek at a big-endian U128.
    """
    (peek_u64_be(offset)?.u128() << 64) or peek_u64_be(offset + 8)?.u128()

  fun box peek_u128_le(offset: USize = 0): U128 ? =>
    """
    Peek at a little-endian U128.
    """
    peek_u64_le(offset)?.u128() or (peek_u64_le(offset + 8)?.u128() << 64)

  fun box peek_i128_be(offset: USize = 0): I128 ? =>
    """
    Peek at a big-endian I129.
    """
    peek_u128_be(offset)?.i128()

  fun box peek_i128_le(offset: USize = 0): I128 ? =>
    """
    Peek at a little-endian I128.
    """
    peek_u128_le(offset)?.i128()

  fun box peek_f32_be(offset: USize = 0): F32 ? =>
    """
    Peek at a big-endian F32.
    """
    F32.from_bits(peek_u32_be(offset)?)

  fun box peek_f32_le(offset: USize = 0): F32 ? =>
    """
    Peek at a little-endian F32.
    """
    F32.from_bits(peek_u32_le(offset)?)

  fun box peek_f64_be(offset: USize = 0): F64 ? =>
    """
    Peek at a big-endian F64.
    """
    F64.from_bits(peek_u64_be(offset)?)

  fun box peek_f64_le(offset: USize = 0): F64 ? =>
    """
    Peek at a little-endian F64.
    """
    F64.from_bits(peek_u64_le(offset)?)

  fun box _peek_byte(offset: USize = 0): U8 ? =>
    """
    Get the byte at the given offset without moving the cursor forward.
    Raise an error if the given offset is not yet available.
    """
    var offset' = offset
    var iter = _chunks.nodes()

    while true do
      let node = iter.next()?
      (var data, var node_offset) = node()?
      offset' = offset' + node_offset

      let data_size = data.size()
      if offset' >= data_size then
        offset' = offset' - data_size
      else
        return data(offset')?
      end
    end

    error

  // TODO: Fix to handle multi-byte sequences
  fun ref _distance_of(byte: U8): USize ? =>
    """
    Get the distance to the first occurrence of the given byte
    """
    if _chunks.size() == 0 then
      error
    end

    var node = _chunks.head()?
    var search_len: USize = 0

    while true do
      (var data, var offset) = node()?

      try
        let len = (search_len + data.find(byte, offset)? + 1) - offset
        search_len = 0
        return len
      end

      search_len = search_len + (data.size() - offset)

      if not node.has_next() then
        break
      end

      node = node.next() as ListNode[(Array[U8] val, USize)]
    end

    error

  fun ref _search_length(): USize ? =>
    """
    Get the length of a pending line. Raise an error if there is no pending
    line.
    """
    _distance_of('\n')?
