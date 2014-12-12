primitive FormatDefault
primitive PrefixDefault
primitive AlignLeft
primitive AlignRight
primitive AlignCenter

type Align is
  ( AlignLeft
  | AlignRight
  | AlignCenter)

interface Stringable box
  """
  Things that can be turned into a String.
  """
  fun box string(fmt: FormatDefault = FormatDefault,
    prefix: PrefixDefault = PrefixDefault, prec: U64 = -1, width: U64 = 0,
    align: Align = AlignLeft, fill: U8 = ' '): String iso^

class String val is Ordered[String box], Stringable
  """
  Strings don't specify an encoding.
  """
  var _size: U64
  var _alloc: U64
  var _ptr: Pointer[U8]

  new create() =>
    """
    A zero length, but valid, string.
    """
    _size = 0
    _alloc = 1
    _ptr = Pointer[U8]._create(1)
    _ptr._update(0, 0)

  new from_cstring(str: Pointer[U8] ref, len: U64 = 0) =>
    """
    The cstring is not copied, so this should be done with care.
    """
    _size = len

    if len == 0 then
      while str._apply(_size) != 0 do
        _size = _size + 1
      end
    end

    _alloc = _size + 1
    _ptr = str

  new copy_cstring(str: Pointer[U8] ref, len: U64 = 0) =>
    """
    The cstring is copied, so this is always safe.
    """
    _size = len

    if len == 0 then
      while str._apply(_size) != 0 do
        _size = _size + 1
      end
    end

    _alloc = _size + 1
    _ptr = Pointer[U8]._create(_alloc)
    @memcpy[Pointer[U8]](_ptr, str, _alloc)

  new prealloc(size: U64) =>
    _size = size
    _alloc = size + 1
    _ptr = Pointer[U8]._create(_alloc)
    _ptr._update(size, 0)

  fun box cstring(): Pointer[U8] tag => _ptr

  fun box size(): U64 => _size

  fun box space(): U64 => _alloc

  fun ref reserve(len: U64): String ref^ =>
    """
    Reserve space for len bytes. An additional byte will be reserved for the
    null terminator.
    """
    if _alloc <= len then
      _alloc = (len + 1).max(8).next_pow2()
      _ptr = _ptr._realloc(_alloc)
    end
    this

  fun ref recalc(): String ref^ =>
    """
    Recalculates the string length. This is only needed if the string is
    changed via an FFI call.
    """
    _size = 0

    while (_size < _alloc) and (_ptr._apply(_size) > 0) do
      _size = _size + 1
    end
    this

  fun box utf32(offset: I64): (U32, U8) ? =>
    """
    Return a UTF32 representation of the character at the given offset and the
    number of bytes needed to encode that character. If the offset does not
    point to the beginning of a valid UTF8 encoding, return 0xFFFD (the unicode
    replacement character) and a length of one. Raise an error if the offset is
    out of bounds.
    """
    let i = offset_to_index(offset)
    let err: (U32, U8) = (0xFFFD, 1)

    if i >= _size then error end
    var c = _ptr._apply(i)

    if c < 0x80 then
      // 1-byte
      (c.u32(), U8(1))
    elseif c < 0xC2 then
      // Stray continuation.
      err
    elseif c < 0xE0 then
      // 2-byte
      if (i + 1) >= _size then
        // Not enough bytes.
        err
      else
        var c2 = _ptr._apply(i + 1)
        if (c2 and 0xC0) != 0x80 then
          // Not a continuation byte.
          err
        else
          (((c.u32() << 6) + c2.u32()) - 0x3080, U8(2))
        end
      end
    elseif c < 0xF0 then
      // 3-byte.
      if (i + 2) >= _size then
        // Not enough bytes.
        err
      else
        var c2 = _ptr._apply(i + 1)
        var c3 = _ptr._apply(i + 2)
        if
          // Not continuation bytes.
          ((c2 and 0xC0) != 0x80) or
          ((c3 and 0xC0) != 0x80) or
          // Overlong encoding.
          ((c == 0xE0) and (c2 < 0xA0))
        then
          err
        else
          (((c.u32() << 12) + (c2.u32() << 6) + c3.u32()) - 0xE2080, U8(3))
        end
      end
    elseif c < 0xF5 then
      // 4-byte.
      if (i + 3) >= _size then
        // Not enough bytes.
        err
      else
        var c2 = _ptr._apply(i + 1)
        var c3 = _ptr._apply(i + 2)
        var c4 = _ptr._apply(i + 3)
        if
          // Not continuation bytes.
          ((c2 and 0xC0) != 0x80) or
          ((c3 and 0xC0) != 0x80) or
          ((c4 and 0xC0) != 0x80) or
          // Overlong encoding.
          ((c == 0xF0) and (c2 < 0x90)) or
          // UTF32 would be > 0x10FFFF.
          ((c == 0xF4) and (c2 >= 0x90))
        then
          err
        else
          (((c.u32() << 18) +
            (c2.u32() << 12) +
            (c3.u32() << 6) +
            c4.u32()) - 0x3C82080, U8(4))
        end
      end
    else
      // UTF32 would be > 0x10FFFF.
      err
    end

  fun box apply(offset: I64): U8 ? =>
    """
    Returns the byte at the given offset. Raise an error if the offset is out
    of bounds.
    """
    let i = offset_to_index(offset)
    if i < _size then _ptr._apply(i) else error end

  fun ref update(offset: I64, value: U8): U8 ? =>
    """
    Changes a byte in the string, returning the previous byte at that offset.
    Raise an error if the offset is out of bounds.
    """
    let i = offset_to_index(offset)

    if i < _size then
      if value == 0 then
        _size = i
      end

      _ptr._update(i, value)
    else
      error
    end

  fun box clone(): String iso^ =>
    """
    Returns a copy of the string.
    """
    let len = _size
    let str = recover String.prealloc(len) end
    var i: U64 = 0

    while i < len do
      let c = _ptr._apply(i)
      str._set(i, c)
      i = i + 1
    end

    consume str

  fun box find(s: String box, offset: I64 = 0, nth: U64 = 0): I64 ? =>
    """
    Return the index of the n-th instance of s in the string starting from the
    beginning. Raise an error if there is no n-th occurence of s or s is empty.
    """
    var i = offset_to_index(offset)
    var steps = nth + 1

    while i < _size do
      var j: U64 = 0

      var same = while j < s._size do
        if _ptr._apply(i + j) != s._ptr._apply(j) then
          break false
        end
        j = j + 1
        true
      else
        false
      end

      if same and ((steps = steps - 1) == 1) then
        return i.i64()
      end

      i = i + 1
    end
    error

  fun box rfind(s: String, offset: I64 = -1, nth: U64 = 0): I64 ? =>
    """
    Return the index of n-th instance of s in the string starting from the end.
    Raise an error if there is no n-th occurence of s or s is empty.
    """
    var i = offset_to_index(offset) - s._size
    var steps = nth + 1

    while i < _size do
      var j: U64 = 0

      var same = while j < s._size do
        if _ptr._apply(i + j) != s._ptr._apply(j) then
          break false
        end
        j = j + 1
        true
      else
        false
      end

      if same and ((steps = steps - 1) == 1) then
        return i.i64()
      end

      i = i - 1
    end
    error

  fun box count(s: String, offset: I64 = 0): U64 =>
    """
    Counts the non-overlapping occurrences of s in the string.
    """
    let j: I64 = (_size - s.size()).i64()
    var i: U64 = 0
    var k = offset

    if j < 0 then
      return 0
    elseif (j == 0) and (this == s) then
      return 1
    end

    try
      while k < j do
        k = find(s, k) + s.size().i64()
        i = i + 1
      end
    end

    i

  fun box at(s: String, offset: I64): Bool =>
    """
    Returns true if the substring s is present at the given offset.
    """
    var i = offset_to_index(offset)
    var j: U64 = 0

    while j < s._size do
      if _ptr._apply(i + j) != s._ptr._apply(j) then
        return false
      end
      j = j + 1
    end
    false

  fun ref delete(offset: I64, len: U64): String ref^ =>
    """
    Delete len bytes at the supplied offset, compacting the string in place.
    """
    var i = offset_to_index(offset)

    if i < _size then
      var n = len.min(_size - i)
      _size = _size - n
      _ptr._delete(i, n, _size - i)
      _ptr._update(_size, 0)
    end
    this

  fun box lower(): String iso^ =>
    """
    Returns a lower case version of the string.
    """
    let len = _size
    let str = recover String.prealloc(len) end
    var i: U64 = 0

    while i < len do
      var c = _ptr._apply(i)

      if (c >= 0x41) and (c <= 0x5A) then
        c = c + 0x20
      end

      str._set(i, c)
      i = i + 1
    end

    consume str

  fun box upper(): String iso^ =>
    """
    Returns an upper case version of the string.
    """
    let len = _size
    let str = recover String.prealloc(len) end
    var i: U64 = 0

    while i < len do
      var c = _ptr._apply(i)

      if (c >= 0x61) and (c <= 0x7A) then
        c = c - 0x20
      end

      str._set(i, c)
      i = i + 1
    end

    consume str

  fun box reverse(): String iso^ =>
    """
    Returns a reversed version of the string.
    """
    let len = _size
    let str = recover String.prealloc(len) end
    var i: U64 = 0

    while i < len do
      let c = _ptr._apply(i)
      str._set(_size - i - 1, c)
      i = i + 1
    end

    consume str

  fun ref reverse_in_place(): String ref^ =>
    if _size > 1 then
      var i: U64 = 0
      var j = _size - 1

      while i < j do
        let x = _ptr._apply(i)
        _ptr._update(i, _ptr._apply(j))
        _ptr._update(j, x)
        i = i + 1
        j = j - 1
      end
    end
    this

  // The range is inclusive.
  fun box substring(from: I64, to: I64): String iso^ =>
    let start = offset_to_index(from)
    let finish = offset_to_index(to).min(_size)
    var str: String iso

    if (start < _size) and (start <= finish) then
      let len = (finish - start) + 1
      str = recover String.prealloc(len) end
      var i = start

      while i <= finish do
        let c = _ptr._apply(i)
        str._set(i - start, c)
        i = i + 1
      end
    else
      str = recover String end
    end

    consume str

  //The range is inclusive.
  fun box cut(from: I64, to: I64): String iso^ =>
    let start = offset_to_index(from)
    let finish = offset_to_index(to).min(_size)
    var str: String iso

    if (start < _size) and (start <= finish) and (finish < _size) then
      let len = _size - ((finish - start) + 1)
      str = recover String.prealloc(len) end
      var i: U64 = 0

      while i < start do
        let c = _ptr._apply(i)
        str._set(i, c)
        i = i + 1
      end

      var j = finish + 1

      while j < _size do
        let c = _ptr._apply(j)
        str._set(start + (j - (finish + 1)), c)
        j = j + 1
      end
    else
      str = recover String end
    end

    consume str

  //The range is inclusive.
  fun ref cut_in_place(from: I64, to: I64): String ref^ =>
    let start = offset_to_index(from)
    let finish = offset_to_index(to).min(_size)

    if (start < _size) and (start <= finish) and (finish < _size) then
      let len = _size - ((finish - start) + 1)
      var j = finish + 1

      while j < _size do
        _ptr._update(start + (j - (finish + 1)), _ptr._apply(j))
        j = j + 1
      end

      _size = len
      _ptr._update(len, 0) //make sure its a C string under the hood.
    end

    this

  fun ref strip_char(c: U8): U64 =>
    var i: U64 = 0
    var n: U64 = 0

    while i < _size do
      if _ptr._apply(i) == c then
        cut_in_place(i.i64(), i.i64())
        n = n + 1
      end
      i = i + 1
    end

    n

  fun box add(that: String box): String =>
    let len = _size + that._size
    let ptr = recover Pointer[U8]._create(len + 1) end
    @memcpy[Pointer[U8]](ptr.u64() + _size, that._ptr, that._size + 1)
    recover String.from_cstring(consume ptr, len) end

  fun box compare(that: String box, n: U64,
    offset: I64 = 0, that_offset: I64 = 0): I32 =>
    """
    Starting at this + offset, compare n characters with that + offset. Return
    zero if the strings are the same. Return a negative number if this is
    less than that, a positive number if this is more than that.
    """
    var i = n
    var j: U64 = offset_to_index(offset)
    var k: U64 = offset_to_index(that_offset)

    if (j + n) > _size then
      return -1
    elseif (k + n) > that._size then
      return 1
    end

    while i > 0 do
      if _ptr._apply(j) != that._ptr._apply(k) then
        return _ptr._apply(j).i32() - that._ptr._apply(k).i32()
      end

      j = j + 1
      k = k + 1
      i = i - 1
    end
    0

  fun box eq(that: String box): Bool =>
    if _size == that._size then
      var i: U64 = 0

      while i < _size do
        if _ptr._apply(i) != that._ptr._apply(i) then
          return false
        end
        i = i + 1
      end
      true
    else
      false
    end

  fun box lt(that: String box): Bool =>
    let len = _size.min(that._size)
    var i: U64 = 0

    while i < len do
      if _ptr._apply(i) < that._ptr._apply(i) then
        return true
      elseif _ptr._apply(i) > that._ptr._apply(i) then
        return false
      end
      i = i + 1
    end

    _size < that._size

  fun box le(that: String box): Bool =>
    let len = _size.min(that._size)
    var i: U64 = 0

    while i < len do
      if _ptr._apply(i) < that._ptr._apply(i) then
        return true
      elseif _ptr._apply(i) > that._ptr._apply(i) then
        return false
      end
      i = i + 1
    end
    _size <= that._size

  fun ref append(that: String box): String ref^ =>
    reserve(_size + that._size)
    @memcpy[Pointer[U8]](_ptr.u64() + _size, that._ptr, that._size + 1)
    _size = _size + that._size
    this

  fun ref append_byte(value: U8): String ref^ =>
    reserve(_size + 1)
    _ptr._update(_size, value)
    _ptr._update(_size + 1, 0)
    _size = _size + 1
    this

  fun ref append_utf32(value: U32): String ref^ =>
    reserve(_size + 4)

    if value < 0x80 then
      _ptr._update(_size, value.u8())
      _size = _size + 1
    elseif value < 0x800 then
      _ptr._update(_size, ((value >> 6) or 0xC0).u8())
      _ptr._update(_size + 1, ((value and 0x3F) or 0x80).u8())
      _size = _size + 2
    elseif value < 0xD800 then
      _ptr._update(_size, ((value >> 12) or 0xE0).u8())
      _ptr._update(_size + 1, (((value >> 6) and 0x3F) or 0x80).u8())
      _ptr._update(_size + 2, ((value and 0x3F) or 0x80).u8())
      _size = _size + 3
    elseif value < 0xE000 then
      // UTF-16 surrogate pairs are not allowed.
      _ptr._update(_size, 0xEF)
      _ptr._update(_size + 1, 0xBF)
      _ptr._update(_size + 2, 0xBD)
      _size = _size + 3
    elseif value < 0x10000 then
      _ptr._update(_size, ((value >> 12) or 0xE0).u8())
      _ptr._update(_size + 1, (((value >> 6) and 0x3F) or 0x80).u8())
      _ptr._update(_size + 2, ((value and 0x3F) or 0x80).u8())
      _size = _size + 3
    elseif value < 0x110000 then
      _ptr._update(_size, ((value >> 18) or 0xF0).u8())
      _ptr._update(_size + 1, (((value >> 12) and 0x3F) or 0x80).u8())
      _ptr._update(_size + 2, (((value >> 6) and 0x3F) or 0x80).u8())
      _ptr._update(_size + 3, ((value and 0x3F) or 0x80).u8())
      _size = _size + 4
    else
      // Code points beyond 0x10FFFF are not allowed.
      _ptr._update(_size, 0xEF)
      _ptr._update(_size + 1, 0xBF)
      _ptr._update(_size + 2, 0xBD)
      _size = _size + 3
    end
    this

  fun box offset_to_index(i: I64): U64 =>
    if i < 0 then i.u64() + _size else i.u64() end

  fun box i8(): I8 => i64().i8()
  fun box i16(): I16 => i64().i16()
  fun box i32(): I32 => i64().i32()

  fun box i64(): I64 =>
    if Platform.windows() then
      @_strtoi64[I64](_ptr, U64(0), I32(10))
    else
      @strtol[I64](_ptr, U64(0), I32(10))
    end

  fun box i128(): I128 =>
    if Platform.windows() then
      i64().i128()
    else
      @strtoll[I128](_ptr, U64(0), I32(10))
    end

  fun box u8(): U8 => u64().u8()
  fun box u16(): U16 => u64().u16()
  fun box u32(): U32 => u64().u32()

  fun box u64(): U64 =>
    if Platform.windows() then
      @_strtoui64[U64](_ptr, U64(0), I32(10))
    else
      @strtoul[U64](_ptr, U64(0), I32(10))
    end

  fun box u128(): U128 =>
    if Platform.windows() then
      u64().u128()
    else
      @strtoull[U128](_ptr, U64(0), I32(10))
    end

  fun box f32(): F32 => @strtof[F32](_ptr, U64(0))
  fun box f64(): F64 => @strtod[F64](_ptr, U64(0))

  fun box hash(): U64 => @hash_block[U64](_ptr, _size)

  fun box string(fmt: FormatDefault = FormatDefault,
    prefix: PrefixDefault = PrefixDefault, prec: U64 = -1, width: U64 = 0,
    align: Align = AlignLeft, fill: U8 = ' '): String iso^
  =>
    let copy_len = _size.min(prec.u64())
    let len = copy_len.max(width.u64())

    let ptr = recover Pointer[U8]._create(len + 1) end

    match align
    | AlignLeft =>
      @memcpy[Pointer[U8]](ptr, _ptr, copy_len)
      @memset[Pointer[U8]](ptr.u64() + copy_len, U32(' '), len - copy_len)
    | AlignRight =>
      @memset[Pointer[U8]](ptr, U32(' '), len - copy_len)
      @memcpy[Pointer[U8]](ptr.u64() + (len - copy_len), _ptr, copy_len)
    | AlignCenter =>
      let half = (len - copy_len) / 2
      @memset[Pointer[U8]](ptr, U32(' '), half)
      @memcpy[Pointer[U8]](ptr.u64() + half, _ptr, copy_len)
      @memset[Pointer[U8]](ptr.u64() + copy_len + half, U32(' '),
        len - copy_len - half)
    end

    ptr._update(len, 0)
    recover String.from_cstring(consume ptr, len) end

  fun ref _set(i: U64, char: U8): U8 =>
    """
    Unsafe update, used internally.
    """
    _ptr._update(i, char)
