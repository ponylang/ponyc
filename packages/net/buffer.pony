use "collections"

class Buffer
  """
  Store network data and provide a parsing interface.
  """
  let _chunks: List[(Array[U8] val, U64)] = _chunks.create()
  var _available: U64 = 0

  fun size(): U64 =>
    """
    Return the number of available bytes.
    """
    _available

  fun ref clear(): Buffer^ =>
    """
    Discard all pending data.
    """
    _chunks.clear()
    _available = 0
    this

  fun ref append(data: Array[U8] val): Buffer^ =>
    """
    Add a chunk of data.
    """
    _available = _available + data.size()
    _chunks.push((data, 0))
    this

  fun ref skip(n: U64): Buffer^ ? =>
    """
    Skip n bytes.
    """
    if _available >= n then
      _available = _available - n
      var rem = n

      while rem > 0 do
        let node = _chunks.head()
        var (data, offset) = node()
        let avail = data.size() - offset

        if avail > rem then
          node() = (data, offset + rem)
          break
        end

        rem = rem - avail
        _chunks.shift()
      end

      this
    else
      error
    end

  fun ref block(len: U64): Array[U8] iso^ ? =>
    """
    Return a block as a contiguous chunk of memory.
    """
    if _available < len then
      error
    end

    _available = _available - len
    var out = recover Array[U8].undefined(len) end
    var i = U64(0)

    while i < len do
      let node = _chunks.head()
      let (data, offset) = node()

      let avail = data.size() - offset
      let need = len - i
      let copy_len = need.min(avail)

      out = recover
        let r = consume ref out
        data.copy_to(r, offset, i, copy_len)
        consume r
      end

      if avail > need then
        node() = (data, offset + need)
        break
      end

      i = i + copy_len
      _chunks.shift()
    end

    consume out

  fun ref line(): String ? =>
    """
    Return a \n or \r\n terminated line as a string. The newline is not
    included in the returned string, but it is removed from the network buffer.
    """
    let len = _line_length()

    _available = _available - len
    var out = recover String(len) end
    var i = U64(0)

    while i < len do
      let node = _chunks.head()
      let (data, offset) = node()

      let avail = data.size() - offset
      let need = len - i
      let copy_len = need.min(avail)

      out.append_array(data, offset, copy_len)

      if avail > need then
        node() = (data, offset + need)
        break
      end

      i = i + copy_len
      _chunks.shift()
    end

    out.truncate(out.size() -
      if (out.size() >= 2) and (out(-2) == '\r') then 2 else 1 end)

    consume out

  fun ref u8(): U8 ? =>
    """
    Get a U8. Raise an error if there isn't enough data.
    """
    if _available >= 1 then
      _byte()
    else
      error
    end

  fun ref i8(): I8 ? =>
    """
    Get an I8.
    """
    u8().i8()

  fun ref u16_be(): U16 ? =>
    """
    Get a big-endian U16.
    """
    if _available >= 2 then
      (u8().u16() << 8) or u8().u16()
    else
      error
    end

  fun ref u16_le(): U16 ? =>
    """
    Get a little-endian U16.
    """
    if _available >= 2 then
      u8().u16() or (u8().u16() << 8)
    else
      error
    end

  fun ref i16_be(): I16 ? =>
    """
    Get a big-endian I16.
    """
    u16_be().i16()

  fun ref i16_le(): I16 ? =>
    """
    Get a little-endian I16.
    """
    u16_le().i16()

  fun ref u32_be(): U32 ? =>
    """
    Get a big-endian U32.
    """
    if _available >= 4 then
      (u16_be().u32() << 16) or u16_be().u32()
    else
      error
    end

  fun ref u32_le(): U32 ? =>
    """
    Get a little-endian U32.
    """
    if _available >= 4 then
      u16_be().u32() or (u16_be().u32() << 16)
    else
      error
    end

  fun ref i32_be(): I32 ? =>
    """
    Get a big-endian I32.
    """
    u32_be().i32()

  fun ref i32_le(): I32 ? =>
    """
    Get a little-endian I32.
    """
    u32_le().i32()

  fun ref u64_be(): U64 ? =>
    """
    Get a big-endian U64.
    """
    if _available >= 8 then
      (u32_be().u64() << 32) or u32_be().u64()
    else
      error
    end

  fun ref u64_le(): U64 ? =>
    """
    Get a little-endian U64.
    """
    if _available >= 8 then
      u32_le().u64() or (u32_le().u64() << 32)
    else
      error
    end

  fun ref i64_be(): I64 ? =>
    """
    Get a big-endian I64.
    """
    u64_be().i64()

  fun ref i64_le(): I64 ? =>
    """
    Get a little-endian I64.
    """
    u64_le().i64()

  fun ref u128_be(): U128 ? =>
    """
    Get a big-endian U128.
    """
    if _available >= 16 then
      (u64_be().u128() << 64) or u64_be().u128()
    else
      error
    end

  fun ref u128_le(): U128 ? =>
    """
    Get a little-endian U128.
    """
    if _available >= 16 then
      u64_le().u128() or (u64_le().u128() << 64)
    else
      error
    end

  fun ref i128_be(): I128 ? =>
    """
    Get a big-endian I129.
    """
    u128_be().i128()

  fun ref i128_le(): I128 ? =>
    """
    Get a little-endian I128.
    """
    u128_le().i128()

  fun ref f32_be(): F32 ? =>
    """
    Get a big-endian F32.
    """
    F32.from_bits(u32_be())

  fun ref f32_le(): F32 ? =>
    """
    Get a little-endian F32.
    """
    F32.from_bits(u32_le())

  fun ref f64_be(): F64 ? =>
    """
    Get a big-endian F64.
    """
    F64.from_bits(u64_be())

  fun ref f64_le(): F64 ? =>
    """
    Get a little-endian F64.
    """
    F64.from_bits(u64_le())

  fun ref _byte(): U8 ? =>
    """
    Get a single byte.
    """
    let node = _chunks.head()
    var (data, offset) = node()
    let r = data(offset)

    offset = offset + 1
    _available = _available - 1

    if offset < data.size() then
      node() = (data, offset)
    else
      _chunks.shift()
    end
    r

  fun ref _line_length(): U64 ? =>
    """
    Get the length of a pending line. Raise an error if there is no pending
    line.
    """
    var len = U64(0)

    for node in _chunks.nodes() do
      var (data, offset) = node()

      try
        return (len + data.find('\n', offset) + 1) - offset
      end

      len = len + (data.size() - offset)
    end
    error
