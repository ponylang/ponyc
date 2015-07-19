class String val is Seq[U8], Ordered[String box], Stringable
  """
  Strings don't specify an encoding.
  """
  var _size: U64
  var _alloc: U64
  var _ptr: Pointer[U8]

  new create(len: U64 = 0) =>
    """
    An empty string. Enough space for len bytes is reserved.
    """
    _size = 0
    _alloc = len + 1
    _ptr = Pointer[U8]._alloc(_alloc)
    _set(0, 0)

  new from_cstring(str: Pointer[U8], len: U64 = 0) =>
    """
    The cstring is not copied. This must be done only with C-FFI functions that
    return null-terminated pony_alloc'd character arrays.
    """
    if str.is_null() then
      _size = 0
      _alloc = 1
      _ptr = Pointer[U8]._alloc(_alloc)
      _set(0, 0)
    else
      _size = len

      if len == 0 then
        while str._apply(_size) != 0 do
          _size = _size + 1
        end
      end

      _alloc = _size + 1
      _ptr = str
    end

  new copy_cstring(str: Pointer[U8] box, len: U64 = 0) =>
    """
    If the cstring is not null terminated and a length isn't specified, this
    can crash. This will only occur if the C-FFI has been used to craft such
    a pointer.
    """
    if str.is_null() then
      _size = 0
      _alloc = 1
      _ptr = Pointer[U8]._alloc(_alloc)
      _set(0, 0)
    else
      _size = len

      if len == 0 then
        while str._apply(_size) != 0 do
          _size = _size + 1
        end
      end

      _alloc = _size + 1
      _ptr = Pointer[U8]._alloc(_alloc)
      str._copy_to(_ptr, _alloc)
    end

  new from_utf32(value: U32) =>
    """
    Create a UTF-8 string from a single UTF-32 code point.
    """
    if value < 0x80 then
      _size = 1
      _alloc = _size + 1
      _ptr = Pointer[U8]._alloc(_alloc)
      _set(0, value.u8())
    elseif value < 0x800 then
      _size = 2
      _alloc = _size + 1
      _ptr = Pointer[U8]._alloc(_alloc)
      _set(0, ((value >> 6) or 0xC0).u8())
      _set(1, ((value and 0x3F) or 0x80).u8())
    elseif value < 0xD800 then
      _size = 3
      _alloc = _size + 1
      _ptr = Pointer[U8]._alloc(_alloc)
      _set(0, ((value >> 12) or 0xE0).u8())
      _set(1, (((value >> 6) and 0x3F) or 0x80).u8())
      _set(2, ((value and 0x3F) or 0x80).u8())
    elseif value < 0xE000 then
      // UTF-16 surrogate pairs are not allowed.
      _size = 3
      _alloc = _size + 1
      _ptr = Pointer[U8]._alloc(_alloc)
      _set(0, 0xEF)
      _set(1, 0xBF)
      _set(2, 0xBD)
      _size = _size + 3
    elseif value < 0x10000 then
      _size = 3
      _alloc = _size + 1
      _ptr = Pointer[U8]._alloc(_alloc)
      _set(0, ((value >> 12) or 0xE0).u8())
      _set(1, (((value >> 6) and 0x3F) or 0x80).u8())
      _set(2, ((value and 0x3F) or 0x80).u8())
    elseif value < 0x110000 then
      _size = 4
      _alloc = _size + 1
      _ptr = Pointer[U8]._alloc(_alloc)
      _set(0, ((value >> 18) or 0xF0).u8())
      _set(1, (((value >> 12) and 0x3F) or 0x80).u8())
      _set(2, (((value >> 6) and 0x3F) or 0x80).u8())
      _set(3, ((value and 0x3F) or 0x80).u8())
    else
      // Code points beyond 0x10FFFF are not allowed.
      _size = 3
      _alloc = _size + 1
      _ptr = Pointer[U8]._alloc(_alloc)
      _set(0, 0xEF)
      _set(1, 0xBF)
      _set(2, 0xBD)
    end
    _set(_size, 0)

  fun cstring(): Pointer[U8] tag =>
    """
    Returns a C compatible pointer to a null terminated string.
    """
    _ptr

  fun size(): U64 =>
    """
    Returns the length of the string.
    """
    _size

  fun codepoints(from: I64 = 0, to: I64 = -1): U64 =>
    """
    Returns the number of unicode code points in the string between the two
    offsets. From and to are inclusive.
    """
    if _size == 0 then
      return 0
    end

    var i = offset_to_index(from)
    var j = offset_to_index(to).min(_size - 1)
    var n = U64(0)

    while i <= j do
      if (_ptr._apply(i) and 0xC0) != 0x80 then
        n = n + 1
      end

      i = i + 1
    end

    n

  fun space(): U64 =>
    """
    Returns the amount of allocated space.
    """
    _alloc

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

  fun ref truncate(len: U64): String ref^ =>
    """
    Truncates the string at the minimum of len and space. Ensures there is a
    null terminator. Does not check for null terminators inside the string.
    """
    _size = len.min(_alloc - 1)
    _set(_size, 0)
    this

  fun utf32(offset: I64): (U32, U8) ? =>
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
      (c.u32(), 1)
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
          (((c.u32() << 6) + c2.u32()) - 0x3080, 2)
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
          (((c.u32() << 12) + (c2.u32() << 6) + c3.u32()) - 0xE2080, 3)
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
            c4.u32()) - 0x3C82080, 4)
        end
      end
    else
      // UTF32 would be > 0x10FFFF.
      err
    end

  fun apply(i: U64): U8 ? =>
    """
    Returns the i-th byte. Raise an error if the index is out of bounds.
    """
    if i < _size then _ptr._apply(i) else error end

  fun ref update(i: U64, value: U8): U8 ? =>
    """
    Change the i-th byte. Raise an error if the index is out of bounds.
    """
    if i < _size then
      _set(i, value)
    else
      error
    end

  fun at_offset(offset: I64): U8 ? =>
    """
    Returns the byte at the given offset. Raise an error if the offset is out
    of bounds.
    """
    this(offset_to_index(offset))

  fun ref update_offset(offset: I64, value: U8): U8 ? =>
    """
    Changes a byte in the string, returning the previous byte at that offset.
    Raise an error if the offset is out of bounds.
    """
    this(offset_to_index(offset)) = value

  fun clone(): String iso^ =>
    """
    Returns a copy of the string.
    """
    let len = _size
    let str = recover String(len) end
    _ptr._copy_to(str._ptr, len + 1)
    str._size = len
    str

  fun find(s: String box, offset: I64 = 0, nth: U64 = 0): I64 ? =>
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

  fun rfind(s: String box, offset: I64 = -1, nth: U64 = 0): I64 ? =>
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

  fun count(s: String box, offset: I64 = 0): U64 =>
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

  fun at(s: String box, offset: I64): Bool =>
    """
    Returns true if the substring s is present at the given offset.
    """
    var i = offset_to_index(offset)

    if (i + s.size()) <= _size then
      @memcmp[I32](_ptr._offset(i), s._ptr, s._size) == 0
    else
      false
    end

  fun ref delete(offset: I64, len: U64): String ref^ =>
    """
    Delete len bytes at the supplied offset, compacting the string in place.
    """
    var i = offset_to_index(offset)

    if i < _size then
      let n = len.min(_size - i)
      _size = _size - n
      _ptr._offset(i)._delete(n, _size - i)
      _set(_size, 0)
    end
    this

  fun substring(from: I64, to: I64): String iso^ =>
    """
    Returns a substring. From and to are inclusive. Returns an empty string if
    nothing is in the range.
    """
    let start = offset_to_index(from)
    let finish = offset_to_index(to).min(_size)

    if (start < _size) and (start <= finish) then
      let len = (finish - start) + 1
      var str = recover String(len) end
      _ptr._offset(start)._copy_to(str._ptr, len)
      str._size = len
      str._set(len, 0)
      str
    else
      recover String end
    end

  fun lower(): String iso^ =>
    """
    Returns a lower case version of the string.
    """
    var s = clone()
    s.lower_in_place()
    s

  fun ref lower_in_place(): String ref^ =>
    """
    Transforms the string to lower case. Currently only knows ASCII case.
    """
    var i: U64 = 0

    while i < _size do
      var c = _ptr._apply(i)

      if (c >= 0x41) and (c <= 0x5A) then
        _set(i, c + 0x20)
      end

      i = i + 1
    end
    this

  fun upper(): String iso^ =>
    """
    Returns an upper case version of the string. Currently only knows ASCII
    case.
    """
    var s = clone()
    s.upper_in_place()
    s

  fun ref upper_in_place(): String ref^ =>
    """
    Transforms the string to upper case.
    """
    var i: U64 = 0

    while i < _size do
      var c = _ptr._apply(i)

      if (c >= 0x61) and (c <= 0x7A) then
        _set(i, c - 0x20)
      end

      i = i + 1
    end
    this

  fun reverse(): String iso^ =>
    """
    Returns a reversed version of the string.
    """
    var s = clone()
    s.reverse_in_place()
    s

  fun ref reverse_in_place(): String ref^ =>
    """
    Reverses the byte order in the string. This needs to be changed to handle
    UTF-8 correctly.
    """
    if _size > 1 then
      var i: U64 = 0
      var j = _size - 1

      while i < j do
        let x = _ptr._apply(i)
        _set(i, _ptr._apply(j))
        _set(j, x)
        i = i + 1
        j = j - 1
      end
    end
    this

  fun ref push(value: U8): String ref^ =>
    """
    Add a byte to the end of the string.
    """
    reserve(_size + 1)
    _set(_size, value)
    _size = _size + 1
    _set(_size, 0)
    this

  fun ref pop(): U8 ? =>
    """
    Remove a byte from the end of the string.
    """
    if _size > 0 then
      _size = _size - 1
      _ptr._offset(_size)._delete(1, 0)
    else
      error
    end

  fun ref unshift(value: U8): String ref^ =>
    """
    Adds a byte to the beginning of the string.
    """
    if value != 0 then
      reserve(_size + 1)
      @memmove[Pointer[U8]](_ptr.u64() + 1, _ptr.u64(), _size + 1)
      _set(0, value)
      _size = _size + 1
    else
      _set(0, 0)
      _size = 0
    end

    this

  fun ref shift(): U8 ? =>
    """
    Removes a byte from the beginning of the string.
    """
    if _size > 0 then
      let value = _ptr._apply(0)
      @memmove[Pointer[U8]](_ptr.u64(), _ptr.u64() + 1, _size)
      _size = _size - 1
      value
    else
      error
    end

  fun ref append(seq: ReadSeq[U8], offset: U64 = 0, len: U64 = -1): String ref^
  =>
    """
    Append the elements from a sequence, starting from the given offset.

    TODO: optimise when it is a string or an array
    """
    if offset >= seq.size() then
      return this
    end

    let copy_len = len.min(seq.size() - offset)
    reserve(_size + copy_len)

    let cap = copy_len + offset
    var i = offset

    try
      while i < cap do
        push(seq(i))
        i = i + 1
      end
    end

    this

  fun ref clear(): String ref^ =>
    """
    Truncate the string to zero length.
    """
    _set(0, 0)
    _size = 0
    this

  fun insert(offset: I64, that: String): String iso^ =>
    """
    Returns a version of the string with the given string inserted at the given
    offset.
    """
    var s = clone()
    s.insert_in_place(offset, that)
    s

  fun ref insert_in_place(offset: I64, that: String box): String ref^ =>
    """
    Inserts the given string at the given offset. Appends the string if the
    offset is out of bounds.
    """
    reserve(_size + that._size)
    var index = offset_to_index(offset).min(_size)
    @memmove[Pointer[U8]](_ptr.u64() + index + that._size,
      _ptr.u64() + index, _size - index)
    that._ptr._copy_to(_ptr._offset(index), that._size)
    _size = _size + that._size
    _set(_size, 0)
    this

  fun ref insert_byte(offset: I64, value: U8): String ref^ =>
    """
    Inserts a byte at the given offset. Appends if the offset is out of bounds.
    """
    reserve(_size + 1)
    var index = offset_to_index(offset).min(_size)
    @memmove[Pointer[U8]](_ptr.u64() + index + 1, _ptr.u64() + index,
      _size - index)
    _set(index, value)
    _size = _size + 1
    _set(_size, 0)
    this

  fun cut(from: I64, to: I64): String iso^ =>
    """
    Returns a version of the string with the given range deleted. The range is
    inclusive.
    """
    var s = clone()
    s.cut_in_place(from, to)
    s

  fun ref cut_in_place(from: I64, to: I64): String ref^ =>
    """
    Cuts the given range out of the string.
    """
    let start = offset_to_index(from)
    let finish = offset_to_index(to).min(_size)

    if (start < _size) and (start <= finish) and (finish < _size) then
      let len = _size - ((finish - start) + 1)
      var j = finish + 1

      while j < _size do
        _set(start + (j - (finish + 1)), _ptr._apply(j))
        j = j + 1
      end

      _size = len
      _set(len, 0)
    end
    this

  fun ref strip(s: String box): U64 =>
    """
    Remove all instances of s from the string. Returns the count of removed
    instances.
    """
    var i: I64 = 0
    var n: U64 = 0

    try
      while true do
        i = find(s, i)
        cut_in_place(i, (i + s.size().i64()) - 1)
        n = n + 1
      end
    end
    n

  fun ref replace(from: String, to: String, n: U64 = 0): String ref^ =>
    """
    Replace up to n occurrences of `from` in `this` with `to`. If n is 0, all
    occurrences will be replaced.
    """
    let from_len = from.size().i64() - 1
    let to_len = to.size().i64()
    var offset = I64(0)
    var occur = U64(0)

    try
      while true do
        offset = find(from, offset)
        cut_in_place(offset, offset + from_len)
        insert_in_place(offset, to)
        offset = offset + to_len
        occur = occur + 1

        if (n > 0) and (occur >= n) then
          break
        end
      end
    end
    this

  fun ref trim(): String ref^ =>
    """
    Trim leading and trailing whitespace. Whitespace is defined as ' ', \t,
    \v, \f, \r, \n.
    """
    if _size > 0 then
      var i = U64(0)

      while i < _size do
        match _ptr._apply(i)
        | ' ' | '\t' | '\v' | '\f' | '\n' | '\r' => None
        else
          break
        end

        i = i + 1
      end

      if i > 0 then
        delete(0, i)
      end
    end

    if _size > 0 then
      var i = _size - 1

      repeat
        match _ptr._apply(i)
        | ' ' | '\t' | '\v' | '\f' | '\n' | '\r' => None
        else
          break
        end
      until (i = i - 1) == 0 end

      truncate(i + 1)
    end

    this

  fun add(that: String box): String =>
    """
    Return a string that is a concatenation of this and that.
    """
    let len = _size + that._size
    var s = recover String(len) end
    _ptr._copy_to(s._ptr, _size)
    that._ptr._copy_to(s._ptr._offset_tag(_size), that._size + 1)
    s._size = len
    s

  fun compare(that: String box, n: U64, offset: I64 = 0,
    that_offset: I64 = 0, ignore_case: Bool = false): Compare
  =>
    """
    Starting at this + offset, compare n bytes with that + offset.
    """
    var i = n
    var j: U64 = offset_to_index(offset)
    var k: U64 = offset_to_index(that_offset)

    if (j + n) > _size then
      return Less
    elseif (k + n) > that._size then
      return Greater
    end

    while i > 0 do
      var c1 = _ptr._apply(j)
      var c2 = that._ptr._apply(k)

      if
        not ((c1 == c2) or
          (ignore_case and
            (c1 >= 0x41) and (c1 <= 0x5A) and ((c1 + 0x20) == c2)))
      then
        return if c1.i32() > c2.i32() then Greater else Less end
      end

      j = j + 1
      k = k + 1
      i = i - 1
    end
    Equal

  fun eq(that: String box): Bool =>
    """
    Returns true if the two strings have the same contents.
    """
    if _size == that._size then
      @memcmp[I32](_ptr, that._ptr, _size) == 0
    else
      false
    end

  fun lt(that: String box): Bool =>
    """
    Returns true if this is lexically less than that. Needs to be made UTF-8
    safe.
    """
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

  fun le(that: String box): Bool =>
    """
    Returns true if this is lexically less than or equal to that. Needs to be
    made UTF-8 safe.
    """
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

  fun offset_to_index(i: I64): U64 =>
    if i < 0 then i.u64() + _size else i.u64() end

  fun i8(base: U8 = 0): I8 ? => _to_int[I8](base)
  fun i16(base: U8 = 0): I16 ? => _to_int[I16](base)
  fun i32(base: U8 = 0): I32 ? => _to_int[I32](base)
  fun i64(base: U8 = 0): I64 ? => _to_int[I64](base)
  fun i128(base: U8 = 0): I128 ? => _to_int[I128](base)
  fun u8(base: U8 = 0): U8 ? => _to_int[U8](base)
  fun u16(base: U8 = 0): U16 ? => _to_int[U16](base)
  fun u32(base: U8 = 0): U32 ? => _to_int[U32](base)
  fun u64(base: U8 = 0): U64 ? => _to_int[U64](base)
  fun u128(base: U8 = 0): U128 ? => _to_int[U128](base)

  fun _to_int[A: ((Signed | Unsigned) & Integer[A] box)](base: U8): A ? =>
    """
    Convert the *whole* string to the specified type.
    If there are any other characters in the string, or the integer found is
    out of range for the target type then an error is thrown.
    """
    (let v, let d) = read_int[A](0, base)
    if (d == 0) or (d.u64() != _size) then error end  // Not all of string used
    v

  fun read_int[A: ((Signed | Unsigned) & Integer[A] box)](offset: I64 = 0,
    base: U8 = 0): (A, I64 /* chars used */) ?
  =>
    """
    Read an integer from the specified location in this string. The integer
    value read and the number of characters consumed are reported.
    The base parameter specifies the base to use, 0 indicates using the prefix,
    if any, to detect base 2, 10 or 16.
    If no integer is found at the specified location, then (0, 0) is returned,
    since no characters have been used.
    An integer out of range for the target type throws an error.
    A leading minus is allowed for signed integer types.
    Underscore characters are allowed throughout the integer and are ignored.
    """
    let start_index = offset_to_index(offset)
    var index = start_index
    var value: A = 0
    var had_digit = false

    // Check for leading minus
    let minus = (index < _size) and (_ptr._apply(index) == '-')
    if minus then
      // Named variables need until issue #220 is fixed
      // if A(-1) > A(0) then
      let neg: A = -1
      let zero: A = 0
      if neg > zero then
        // We're reading an unsigned type, negative not allowed, int not found
        return (0, 0)
      end

      index = index + 1
    end

    (let base', let base_chars) = _read_int_base[A](base, index)
    index = index + base_chars

    // Process characters
    while index < _size do
      let char: A = A(0).from[U8](_ptr._apply(index))
      if char == '_' then
        index = index + 1
        continue
      end

      let digit =
        if (char >= '0') and (char <= '9') then
          char - '0'
        elseif (char >= 'A') and (char <= 'Z') then
          (char - 'A') + 10
        elseif (char >= 'a') and (char <= 'z') then
          (char - 'a') + 10
        else
          break
        end

      if digit >= base' then
        break
      end

      let new_value: A = (value * base') + digit

      if (new_value / base') != value then
        // Overflow
        error
      end

      value = new_value
      had_digit = true
      index = index + 1
    end

    if minus then value = -value end

    // Check result
    if not had_digit then
      // No integer found
      return (0, 0)
    end

    // Success
    (value, (index - start_index).i64())

  fun _read_int_base[A: ((Signed | Unsigned) & Integer[A] box)]
    (base: U8, index: U64): (A, U64 /* chars used */)
  =>
    """
    Determine the base of an integer starting at the specified index.
    If a non-0 base is given use that. If given base is 0 read the base
    specifying prefix, if any, to detect base 2 or 16.
    If no base is specified and no prefix is found default to decimal.
    Note that a leading 0 does NOT imply octal.
    Report the base found and the number of characters in the prefix.
    """
    if base > 0 then
      return (A(0).from[U8](base), 0)
    end

    // Determine base from prefix
    if (index + 2) >= _size then
      // Not enough characters, must be decimal
      return (10, 0)
    end

    let lead_char = _ptr._apply(index)
    let base_char = _ptr._apply(index + 1) and not 0x20

    if (lead_char == '0') and (base_char == 'B') then
      return (2, 2)
    end

    if (lead_char == '0') and (base_char == 'X') then
      return (16, 2)
    end

    // No base specified, default to decimal
    (10, 0)

  fun f32(offset: I64 = 0): F32 =>
    var index = offset_to_index(offset)

    if index < _size then
      @strtof[F32](_ptr.u64() + index, U64(0))
    else
      F32(0)
    end

  fun f64(offset: I64 = 0): F64 =>
    var index = offset_to_index(offset)

    if index < _size then
      @strtod[F64](_ptr.u64() + index, U64(0))
    else
      F64(0)
    end

  fun hash(): U64 =>
    @hash_block[U64](_ptr, _size)

  fun string(fmt: FormatDefault = FormatDefault,
    prefix: PrefixDefault = PrefixDefault, prec: U64 = -1, width: U64 = 0,
    align: Align = AlignLeft, fill: U32 = ' '): String iso^
  =>
    // TODO: fill character
    let copy_len = _size.min(prec.u64())
    let len = copy_len.max(width.u64())
    let str = recover String(len) end

    match align
    | AlignLeft =>
      _ptr._copy_to(str._ptr, copy_len)
      @memset[Pointer[U8]](str._ptr.u64() + copy_len, U32(' '), len - copy_len)
    | AlignRight =>
      @memset[Pointer[U8]](str._ptr, U32(' '), len - copy_len)
      _ptr._copy_to(str._ptr._offset_tag(len - copy_len), copy_len)
    | AlignCenter =>
      let half = (len - copy_len) / 2
      @memset[Pointer[U8]](str._ptr, U32(' '), half)
      _ptr._copy_to(str._ptr._offset_tag(half), copy_len)
      @memset[Pointer[U8]](str._ptr.u64() + copy_len + half, U32(' '),
        len - copy_len - half)
    end

    str._size = len
    str._set(len, 0)
    str

  // fun format(args: Array[String] box): String ? =>
  //   recover
  //     var s = String.
  //   var i: U64 = 0
  //
  //   while i < _size do

  fun values(): StringBytes^ =>
    """
    Return an iterator over the bytes in the string.
    """
    StringBytes(this)

  fun runes(): StringRunes^ =>
    """
    Return an iterator over the codepoints in the string.
    """
    StringRunes(this)

  fun ref _set(i: U64, value: U8): U8 =>
    """
    Unsafe update, used internally.
    """
    _ptr._update(i, value)

class StringBytes is Iterator[U8]
  let _string: String box
  var _i: U64

  new create(string: String box) =>
    _string = string
    _i = 0

  fun has_next(): Bool =>
    _i < _string.size()

  fun ref next(): U8 ? =>
    _string(_i = _i + 1)

class StringRunes is Iterator[U32]
  let _string: String box
  var _i: U64

  new create(string: String box) =>
    _string = string
    _i = 0

  fun has_next(): Bool =>
    _i < _string.size()

  fun ref next(): U32 ? =>
    (let rune, let len) = _string.utf32(_i.i64())
    _i = _i + len.u64()
    rune
