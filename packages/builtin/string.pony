use @memcmp[I32](dst: Pointer[U8] box, src: Pointer[U8] box, len: USize)
use @memset[Pointer[None]](dst: Pointer[None], set: U32, len: USize)
use @memmove[Pointer[None]](dst: Pointer[None], src: Pointer[None], len: USize)


class val String is (Seq[U8] & Comparable[String box] & Stringable)
  """
  Strings don't specify an encoding.
  """
  var _size: USize
  var _alloc: USize
  var _ptr: Pointer[U8]

  new create(len: USize = 0) =>
    """
    An empty string. Enough space for len bytes is reserved.
    """
    _size = 0
    _alloc = len.min(len.max_value() - 1) + 1
    _ptr = Pointer[U8]._alloc(_alloc)
    _set(0, 0)

  new from_cstring(str: Pointer[U8], len: USize = 0) =>
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

  new copy_cstring(str: Pointer[U8] box, len: USize = 0) =>
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

  fun size(): USize =>
    """
    Returns the length of the string.
    """
    _size

  fun codepoints(from: ISize = 0, to: ISize = ISize.max_value()): USize =>
    """
    Returns the number of unicode code points in the string between the two
    offsets. Index range [`from` .. `to`) is half-open.
    """
    if _size == 0 then
      return 0
    end

    var i = offset_to_index(from)
    var j = offset_to_index(to).min(_size)
    var n = USize(0)

    while i < j do
      if (_ptr._apply(i) and 0xC0) != 0x80 then
        n = n + 1
      end

      i = i + 1
    end

    n

  fun space(): USize =>
    """
    Returns the space available for data, not including the null terminator.
    """
    _alloc - 1

  fun ref reserve(len: USize): String ref^ =>
    """
    Reserve space for len bytes. An additional byte will be reserved for the
    null terminator.
    """
    if _alloc <= len then
      _alloc = len.min(len.max_value() - 1) + 1
      _ptr = _ptr._realloc(_alloc)
    end
    this

  fun ref recalc(): String ref^ =>
    """
    Recalculates the string length. This is only needed if the string is
    changed via an FFI call. If the string is not null terminated at the
    allocated length, a null is added.
    """
    _size = 0

    while (_size < _alloc) and (_ptr._apply(_size) > 0) do
      _size = _size + 1
    end

    if _size == _alloc then
      _size = _size - 1
      _set(_size, 0)
    end

    this

  fun ref truncate(len: USize): String ref^ =>
    """
    Truncates the string at the minimum of len and space. Ensures there is a
    null terminator. Does not check for null terminators inside the string.
    """
    _size = len.min(_alloc - 1)
    _set(_size, 0)
    this

  fun utf32(offset: ISize): (U32, U8) ? =>
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

  fun apply(i: USize): U8 ? =>
    """
    Returns the i-th byte. Raise an error if the index is out of bounds.
    """
    if i < _size then _ptr._apply(i) else error end

  fun ref update(i: USize, value: U8): U8 ? =>
    """
    Change the i-th byte. Raise an error if the index is out of bounds.
    """
    if i < _size then
      _set(i, value)
    else
      error
    end

  fun at_offset(offset: ISize): U8 ? =>
    """
    Returns the byte at the given offset. Raise an error if the offset is out
    of bounds.
    """
    this(offset_to_index(offset))

  fun ref update_offset(offset: ISize, value: U8): U8 ? =>
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

  fun find(s: String box, offset: ISize = 0, nth: USize = 0): ISize ? =>
    """
    Return the index of the n-th instance of s in the string starting from the
    beginning. Raise an error if there is no n-th occurence of s or s is empty.
    """
    var i = offset_to_index(offset)
    var steps = nth + 1

    while i < _size do
      var j: USize = 0

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
        return i.isize()
      end

      i = i + 1
    end
    error

  fun rfind(s: String box, offset: ISize = -1, nth: USize = 0): ISize ? =>
    """
    Return the index of n-th instance of s in the string starting from the end.
    Raise an error if there is no n-th occurence of s or s is empty.
    """
    var i = offset_to_index(offset) - s._size
    var steps = nth + 1

    while i < _size do
      var j: USize = 0

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
        return i.isize()
      end

      i = i - 1
    end
    error

  fun count(s: String box, offset: ISize = 0): USize =>
    """
    Counts the non-overlapping occurrences of s in the string.
    """
    let j: ISize = (_size - s.size()).isize()
    var i: USize = 0
    var k = offset

    if j < 0 then
      return 0
    elseif (j == 0) and (this == s) then
      return 1
    end

    try
      while k < j do
        k = find(s, k) + s.size().isize()
        i = i + 1
      end
    end

    i

  fun at(s: String box, offset: ISize = 0): Bool =>
    """
    Returns true if the substring s is present at the given offset.
    """
    var i = offset_to_index(offset)

    if (i + s.size()) <= _size then
      @memcmp(_ptr._offset(i), s._ptr, s._size) == 0
    else
      false
    end

  fun ref delete(offset: ISize, len: USize = 1): String ref^ =>
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

  fun substring(from: ISize, to: ISize = ISize.max_value()): String iso^ =>
    """
    Returns a substring. Index range [`from` .. `to`) is half-open.
    Returns an empty string if nothing is in the range.
    """
    let start = offset_to_index(from)
    let finish = offset_to_index(to).min(_size)

    if (start < _size) and (start < finish) then
      let len = finish - start
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
    var i: USize = 0

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
    var i: USize = 0

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
      var i: USize = 0
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
      @memmove(_ptr.usize() + 1, _ptr.usize(), _size + 1)
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
      @memmove(_ptr.usize(), _ptr.usize() + 1, _size)
      _size = _size - 1
      value
    else
      error
    end

  fun ref append(seq: ReadSeq[U8], offset: USize = 0, len: USize = -1):
    String ref^
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

  fun insert(offset: ISize, that: String): String iso^ =>
    """
    Returns a version of the string with the given string inserted at the given
    offset.
    """
    var s = clone()
    s.insert_in_place(offset, that)
    s

  fun ref insert_in_place(offset: ISize, that: String box): String ref^ =>
    """
    Inserts the given string at the given offset. Appends the string if the
    offset is out of bounds.
    """
    reserve(_size + that._size)
    var index = offset_to_index(offset).min(_size)
    @memmove(_ptr.usize() + index + that._size,
      _ptr.usize() + index, _size - index)
    that._ptr._copy_to(_ptr._offset(index), that._size)
    _size = _size + that._size
    _set(_size, 0)
    this

  fun ref insert_byte(offset: ISize, value: U8): String ref^ =>
    """
    Inserts a byte at the given offset. Appends if the offset is out of bounds.
    """
    reserve(_size + 1)
    var index = offset_to_index(offset).min(_size)
    @memmove(_ptr.usize() + index + 1, _ptr.usize() + index,
      _size - index)
    _set(index, value)
    _size = _size + 1
    _set(_size, 0)
    this

  fun cut(from: ISize, to: ISize = ISize.max_value()): String iso^ =>
    """
    Returns a version of the string with the given range deleted.
    Index range [`from` .. `to`) is half-open.
    """
    var s = clone()
    s.cut_in_place(from, to)
    s

  fun ref cut_in_place(from: ISize, to: ISize = ISize.max_value()): String ref^
  =>
    """
    Cuts the given range out of the string.
    Index range [`from` .. `to`) is half-open.
    """
    let start = offset_to_index(from)
    let finish = offset_to_index(to).min(_size)

    if (start < _size) and (start < finish) and (finish <= _size) then
      let fragment_len = finish - start
      let new_size = _size - fragment_len
      var i = start

      while i < new_size do
        _set(i, _ptr._apply(i + fragment_len))
        i = i + 1
      end

      _size = _size - fragment_len
      _set(_size, 0)
    end
    this

  fun ref remove(s: String box): USize =>
    """
    Remove all instances of s from the string. Returns the count of removed
    instances.
    """
    var i: ISize = 0
    var n: USize = 0

    try
      while true do
        i = find(s, i)
        cut_in_place(i, i + s.size().isize())
        n = n + 1
      end
    end
    n

  fun ref replace(from: String box, to: String box, n: USize = 0):
    String ref^
  =>
    """
    Replace up to n occurrences of `from` in `this` with `to`. If n is 0, all
    occurrences will be replaced.
    """
    let from_len = from.size().isize()
    let to_len = to.size().isize()
    var offset = ISize(0)
    var occur = USize(0)

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

  fun split(delim: String = " \t\v\f\r\n", n: USize = 0): Array[String] iso^ =>
    """
    Split the string into an array of strings. Any character in the delimiter
    string is accepted as a delimiter. If `n > 0`, then the split count is
    limited to n.

    Adjacent delimiters result in a zero length entry in the array. For
    example, `"1,,2".split(",") => ["1", "", "2"]`.
    """
    let result = recover Array[String] end

    if _size > 0 then
      let chars = Array[U32](delim.size())

      for rune in delim.runes() do
        chars.push(rune)
      end

      var cur = recover String end
      var i = USize(0)
      var occur = USize(0)

      try
        while i < _size do
          (let c, let len) = utf32(i.isize())

          try
            // If we find a delimeter, add the current string to the array.
            chars.find(c)
            occur = occur + 1

            if (n > 0) and (occur >= n) then
              break
            end

            result.push(cur = recover String end)
          else
            // Add bytes to the current string.
            var j = U8(0)

            while j < len do
              cur.push(_ptr._apply(i + j.usize()))
              j = j + 1
            end
          end

          i = i + len.usize()
        end
      end

      // Add all remaining bytes to the current string.
      while i < _size do
        cur.push(_ptr._apply(i))
        i = i + 1
      end

      result.push(consume cur)
    end

    consume result

  fun ref strip(s: String box = " \t\v\f\r\n"): String ref^ =>
    """
    Remove all leading and trailing characters from the string that are in s.
    """
    lstrip(s).rstrip(s)

  fun ref rstrip(s: String box = " \t\v\f\r\n"): String ref^ =>
    """
    Remove all trailing characters within the string that are in s. By default,
    trailing whitespace is removed.
    """
    if _size > 0 then
      let chars = Array[U32](s.size())
      var i = _size - 1

      for rune in s.runes() do
        chars.push(rune)
      end

      repeat
        try
          match utf32(i.isize())
          | (0xFFFD, 1) => None
          | (let c: U32, _) => chars.find(c)
          end
        else
          break
        end
      until (i = i - 1) == 0 end

      truncate(i + 1)
    end

    this

  fun ref lstrip(s: String box = " \t\v\f\r\n"): String ref^ =>
    """
    Remove all leading characters within the string that are in s. By default,
    leading whitespace is removed.
    """
    if _size > 0 then
      let chars = Array[U32](s.size())
      var i = USize(0)

      for rune in s.runes() do
        chars.push(rune)
      end

      while i < _size do
        try
          (let c, let len) = utf32(i.isize())
          chars.find(c)
          i = i + len.usize()
        else
          break
        end
      end

      if i > 0 then
        delete(0, i)
      end
    end

    this

  fun iso _append(s: String box): String iso^ =>
    reserve(s._size + _size)
    s._ptr._copy_to(_ptr._offset_tag(_size), s._size + 1) // + 1 for null
    _size = s._size + _size
    consume this

  fun add(that: String box): String =>
    """
    Return a string that is a concatenation of this and that.
    """
    let len = _size + that._size
    var s = recover String(len) end
    (consume s)._append(this)._append(that)

  fun join(data: ReadSeq[Stringable]): String iso^ =>
    """
    Return a string that is a concatenation of the strings in data, using this
    as a separator.
    """
    var buf = recover String end
    var first = true
    for v in data.values() do
      if not first then
        buf = (consume buf)._append(this)
      end
      first = false
      buf.append(v.string())
    end
    buf

  fun compare(that: String box): Compare =>
    """
    Lexically compare two strings.
    """
    compare_sub(that, _size)

  fun compare_sub(that: String box, n: USize, offset: ISize = 0,
    that_offset: ISize = 0, ignore_case: Bool = false): Compare
  =>
    """
    Starting at this + offset, compare n bytes with that + offset.
    """
    var i = n
    var j: USize = offset_to_index(offset)
    var k: USize = that.offset_to_index(that_offset)

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
          (ignore_case and ((c1 or 0x20) == (c2 or 0x20)) and
            ((c1 or 0x20) >= 'a') and ((c1 or 0x20) <= 'z')))
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
      @memcmp(_ptr, that._ptr, _size) == 0
    else
      false
    end

  fun lt(that: String box): Bool =>
    """
    Returns true if this is lexically less than that. Needs to be made UTF-8
    safe.
    """
    let len = _size.min(that._size)
    var i: USize = 0

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
    var i: USize = 0

    while i < len do
      if _ptr._apply(i) < that._ptr._apply(i) then
        return true
      elseif _ptr._apply(i) > that._ptr._apply(i) then
        return false
      end
      i = i + 1
    end
    _size <= that._size

  fun offset_to_index(i: ISize): USize =>
    if i < 0 then i.usize() + _size else i.usize() end

  fun i8(base: U8 = 0): I8 ? => _to_int[I8](base)
  fun i16(base: U8 = 0): I16 ? => _to_int[I16](base)
  fun i32(base: U8 = 0): I32 ? => _to_int[I32](base)
  fun i64(base: U8 = 0): I64 ? => _to_int[I64](base)
  fun i128(base: U8 = 0): I128 ? => _to_int[I128](base)
  fun ilong(base: U8 = 0): ILong ? => _to_int[ILong](base)
  fun isize(base: U8 = 0): ISize ? => _to_int[ISize](base)
  fun u8(base: U8 = 0): U8 ? => _to_int[U8](base)
  fun u16(base: U8 = 0): U16 ? => _to_int[U16](base)
  fun u32(base: U8 = 0): U32 ? => _to_int[U32](base)
  fun u64(base: U8 = 0): U64 ? => _to_int[U64](base)
  fun u128(base: U8 = 0): U128 ? => _to_int[U128](base)
  fun ulong(base: U8 = 0): ULong ? => _to_int[ULong](base)
  fun usize(base: U8 = 0): USize ? => _to_int[USize](base)

  fun _to_int[A: ((Signed | Unsigned) & Integer[A] val)](base: U8): A ? =>
    """
    Convert the *whole* string to the specified type.
    If there are any other characters in the string, or the integer found is
    out of range for the target type then an error is thrown.
    """
    (let v, let d) = read_int[A](0, base)
    // Check the whole string is used
    if (d == 0) or (d.usize() != _size) then error end
    v

  fun read_int[A: ((Signed | Unsigned) & Integer[A] val)](offset: ISize = 0,
    base: U8 = 0): (A, USize /* chars used */) ?
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
    (value, index - start_index)

  fun _read_int_base[A: ((Signed | Unsigned) & Integer[A] val)]
    (base: U8, index: USize): (A, USize /* chars used */)
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

  fun f32(offset: ISize = 0): F32 =>
    var index = offset_to_index(offset)

    if index < _size then
      @strtof[F32](_ptr.usize() + index, USize(0))
    else
      F32(0)
    end

  fun f64(offset: ISize = 0): F64 =>
    var index = offset_to_index(offset)

    if index < _size then
      @strtod[F64](_ptr.usize() + index, USize(0))
    else
      F64(0)
    end

  fun hash(): U64 =>
    @hash_block[U64](_ptr, _size)

  fun string(fmt: FormatDefault = FormatDefault,
    prefix: PrefixDefault = PrefixDefault, prec: USize = -1, width: USize = 0,
    align: Align = AlignLeft, fill: U32 = ' '): String iso^
  =>
    // TODO: fill character
    let copy_len = _size.min(prec.usize())
    let len = copy_len.max(width.usize())
    let str = recover String(len) end

    match align
    | AlignLeft =>
      _ptr._copy_to(str._ptr, copy_len)
      @memset(str._ptr.usize() + copy_len, U32(' '), len - copy_len)
    | AlignRight =>
      @memset(str._ptr, U32(' '), len - copy_len)
      _ptr._copy_to(str._ptr._offset_tag(len - copy_len), copy_len)
    | AlignCenter =>
      let half = (len - copy_len) / 2
      @memset(str._ptr, U32(' '), half)
      _ptr._copy_to(str._ptr._offset_tag(half), copy_len)
      @memset(str._ptr.usize() + copy_len + half, U32(' '),
        len - copy_len - half)
    end

    str._size = len
    str._set(len, 0)
    str

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

  fun ref _set(i: USize, value: U8): U8 =>
    """
    Unsafe update, used internally.
    """
    _ptr._update(i, value)

class StringBytes is Iterator[U8]
  let _string: String box
  var _i: USize

  new create(string: String box) =>
    _string = string
    _i = 0

  fun has_next(): Bool =>
    _i < _string.size()

  fun ref next(): U8 ? =>
    _string(_i = _i + 1)

class StringRunes is Iterator[U32]
  let _string: String box
  var _i: USize

  new create(string: String box) =>
    _string = string
    _i = 0

  fun has_next(): Bool =>
    _i < _string.size()

  fun ref next(): U32 ? =>
    (let rune, let len) = _string.utf32(_i.isize())
    _i = _i + len.usize()
    rune
