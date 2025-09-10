use @memcmp[I32](dst: Pointer[None] tag, src: Pointer[None] tag, len: USize)
use @memset[Pointer[None]](dst: Pointer[None], set: U32, len: USize)
use @memmove[Pointer[None]](dst: Pointer[None], src: Pointer[None], len: USize)
use @strtof[F32](nptr: Pointer[U8] tag, endptr: Pointer[Pointer[U8] box] ref)
use @strtod[F64](nptr: Pointer[U8] tag, endptr: Pointer[Pointer[U8] box] ref)
use @pony_os_clear_errno[None]()
use @pony_os_errno[I32]()

class val String is (Seq[U8] & Comparable[String box] & Stringable)
  """
  A String is an ordered collection of bytes.

  Strings don't specify an encoding.

  Example usage of some common String methods:

```pony
actor Main
  new create(env: Env) =>
    try
      // construct a new string
      let str = "Hello"

      // make an uppercased version
      let str_upper = str.upper()
      // make a reversed version
      let str_reversed = str.reverse()

      // add " world" to the end of our original string
      let str_new = str.add(" world")

      // count occurrences of letter "l"
      let count = str_new.count("l")

      // find first occurrence of letter "w"
      let first_w = str_new.find("w") ?
      // find first occurrence of letter "d"
      let first_d = str_new.find("d") ?

      // get substring capturing "world"
      let substr = str_new.substring(first_w, first_d+1)
      // clone substring
      let substr_clone = substr.clone()

      // print our substr
      env.out.print(consume substr)
  end
```
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

  new val from_array(data: Array[U8] val) =>
    """
    Create a string from an array, reusing the underlying data pointer.
    """
    _size = data.size()
    _alloc = data.space()
    _ptr = data.cpointer()._unsafe()

  new iso from_iso_array(data: Array[U8] iso) =>
    """
    Create a string from an array, reusing the underlying data pointer
    """
    _size = data.size()
    _alloc = data.space()
    _ptr = (consume data).cpointer()._unsafe()
    if _alloc > _size then
      _set(_size, 0)
    end

  new from_cpointer(str: Pointer[U8], len: USize, alloc: USize = 0) =>
    """
    Return a string from binary pointer data without making a
    copy. This must be done only with C-FFI functions that return
    pony_alloc'd character arrays. If a null pointer is given then an
    empty string is returned.
    """
    if str.is_null() then
      _size = 0
      _alloc = 1
      _ptr = Pointer[U8]._alloc(_alloc)
      _set(0, 0)
    else
      _size = len
      _alloc = alloc.max(_size.min(len.max_value() - 1))
      _ptr = str
    end

  new from_cstring(str: Pointer[U8]) =>
    """
    Return a string from a pointer to a null-terminated cstring
    without making a copy. The data is not copied. This must be done
    only with C-FFI functions that return pony_alloc'd character
    arrays. The pointer is scanned for the first null byte, which will
    be interpreted as the null terminator. Note that the scan is
    unbounded; the pointed to data must be null-terminated within
    the allocated array to preserve memory safety. If a null pointer
    is given then an empty string is returned.
    """
    if str.is_null() then
      _size = 0
      _alloc = 1
      _ptr = Pointer[U8]._alloc(_alloc)
      _set(0, 0)
    else
      var i: USize = 0

      while str._apply(i) != 0 do
        i = i + 1
      end

      _size = i
      _alloc = i + 1
      _ptr = str
    end

  new from_utf32(value: U32) =>
    """
    Create a UTF-8 string from a single UTF-32 code point.
    """
    let encoded = _UTF32Encoder.encode(value)
    _size = encoded._1
    _alloc = _size + 1
    _ptr = Pointer[U8]._alloc(_alloc)
    _set(0, encoded._2)
    if encoded._1 > 1 then
      _set(1, encoded._3)
      if encoded._1 > 2 then
        _set(2, encoded._4)
        if encoded._1 > 3 then
          _set(3, encoded._5)
        end
      end
    end
    _set(_size, 0)

  fun copy_cpointer(ptr: Pointer[U8] box, len: USize): String iso^ =>
    """
    Create a string by copying a fixed number of bytes from a pointer.
    """
    let str: String iso = recover iso String(len + 1) end
    if not ptr.is_null() then
      ptr._copy_to(str._ptr._unsafe(), len)
      str._set(len, 0)
      str.recalc()
    end
    consume str

  fun copy_cstring(ptr: Pointer[U8] box): String iso^ =>
    """
    Create a string by copying a null-terminated C string. Note that
    the scan is unbounded; the pointed to data must be null-terminated
    within the allocated array to preserve memory safety. If a null
    pointer is given then an empty string is returned.
    """
    var len: USize =
      if not ptr.is_null() then
        var i: USize = 0
        while ptr._apply(i) != 0 do
          i = i + 1
        end
        i
      else
        0
      end

    var str: String iso = recover iso String(len + 1) end
    ptr._copy_to(str._ptr._unsafe(), len)
    str._set(len, 0)
    str.recalc()
    consume str

  fun ref push_utf32(value: U32) =>
    """
    Push a UTF-32 code point.
    """
    let encoded = _UTF32Encoder.encode(value)
    let i = _size
    _size = _size + encoded._1
    reserve(_size)
    _set(i, encoded._2)
    if encoded._1 > 1 then
      _set(i + 1, encoded._3)
      if encoded._1 > 2 then
        _set(i + 2, encoded._4)
        if encoded._1 > 3 then
          _set(i + 3, encoded._5)
        end
      end
    end
    _set(_size, 0)

  fun _copy_to(ptr: Pointer[U8] ref, copy_len: USize,
    from_offset: USize = 0, to_offset: USize = 0) =>
    """
    Copy `copy_len` bytes from this to that at specified offsets.
    """
    _ptr._offset(from_offset)._copy_to(ptr._offset(to_offset), copy_len)

  fun cpointer(offset: USize = 0): Pointer[U8] tag =>
    """
    Returns a C compatible pointer to the underlying string allocation.
    """
    _ptr._offset(offset)

  fun cstring(): Pointer[U8] tag =>
    """
    Returns a C compatible pointer to a null-terminated version of the
    string, safe to pass to an FFI function that doesn't accept a size
    argument, expecting a null-terminator. If the underlying string
    is already null terminated, this is returned; otherwise the string
    is copied into a new, null-terminated allocation.
    """
    if is_null_terminated() then
      return _ptr
    end

    let ptr = Pointer[U8]._alloc(_size + 1)
    _ptr._copy_to(ptr._unsafe(), _size)
    ptr._update(_size, 0)
    ptr

  fun val array(): Array[U8] val =>
    """
    Returns an Array[U8] that reuses the underlying data pointer.
    """
    recover
      Array[U8].from_cpointer(_ptr._unsafe(), _size, _alloc)
    end

  fun iso iso_array(): Array[U8] iso^ =>
    """
    Returns an Array[U8] iso that reuses the underlying data pointer.
    """
    recover
      Array[U8].from_cpointer(_ptr._unsafe(), _size, _alloc)
    end

  fun size(): USize =>
    """
    Returns the length of the string data in bytes.
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
    let j = offset_to_index(to).min(_size)
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
    if is_null_terminated() then _alloc - 1 else _alloc end

  fun ref reserve(len: USize) =>
    """
    Reserve space for len bytes. An additional byte will be reserved for the
    null terminator.
    """
    if _alloc <= len then
      let max = len.max_value() - 1
      let min_alloc = len.min(max) + 1
      if min_alloc <= (max / 2) then
        _alloc = min_alloc.next_pow2()
      else
        _alloc = min_alloc.min(max)
      end
      _ptr = _ptr._realloc(_alloc, _size)
    end

  fun ref compact() =>
    """
    Try to remove unused space, making it available for garbage collection. The
    request may be ignored. The string is returned to allow call chaining.
    """
    if (_size + 1) <= 512 then
      if (_size + 1).next_pow2() != _alloc.next_pow2() then
        _alloc = (_size + 1).next_pow2()
        let old_ptr = _ptr = Pointer[U8]._alloc(_alloc)
        old_ptr._copy_to(_ptr, _size)
        _set(_size, 0)
      end
    elseif (_size + 1) < _alloc then
      _alloc = (_size + 1)
      let old_ptr = _ptr = Pointer[U8]._alloc(_alloc)
      old_ptr._copy_to(_ptr, _size)
      _set(_size, 0)
    end

  fun ref recalc() =>
    """
    Recalculates the string length. This is only needed if the string is
    changed via an FFI call. If a null terminator byte is not found within the
    allocated length, the size will not be changed.
    """
    var s: USize = 0

    while (s < _alloc) and (_ptr._apply(s) > 0) do
      s = s + 1
    end

    if s != _alloc then
      _size = s
    end

  fun ref truncate(len: USize) =>
    """
    Truncates the string at the minimum of len and space. Ensures there is a
    null terminator. Does not check for null terminators inside the string.

    Note that memory is not freed by this operation.
    """
    if len >= _alloc then
      _size = len.min(_alloc)
      reserve(_alloc + 1)
    else
      _size = len.min(_alloc - 1)
    end

    _set(_size, 0)

  fun ref trim_in_place(from: USize = 0, to: USize = -1) =>
    """
    Trim the string to a portion of itself, covering `from` until `to`.
    Unlike slice, the operation does not allocate a new string nor copy
    elements.
    """
    let last = _size.min(to)
    let offset = last.min(from)
    let size' = last - offset

    // use the new size' for alloc if we're not including the last used byte
    // from the original data and only include the extra allocated bytes if
    // we're including the last byte.
    _alloc = if last == _size then _alloc - offset else size' end

    _size = size'

    // if _alloc == 0 then we've trimmed all the memory originally allocated.
    // if we do _ptr._offset, we will spill into memory not allocated/owned
    // by this string and could potentially cause a segfault if we cross
    // a pagemap boundary into a pagemap address that hasn't been allocated
    // yet when `reserve` is called next.
    if _alloc == 0 then
      _ptr = Pointer[U8]
    else
      _ptr = _ptr._offset(offset)
    end

  fun val trim(from: USize = 0, to: USize = -1): String val =>
    """
    Return a shared portion of this string, covering `from` until `to`.
    Both the original and the new string are immutable, as they share memory.
    The operation does not allocate a new string pointer nor copy elements.
    """
    let last = _size.min(to)
    let offset = last.min(from)

    recover
      let size' = last - offset

      // use the new size' for alloc if we're not including the last used byte
      // from the original data and only include the extra allocated bytes if
      // we're including the last byte.
      let alloc = if last == _size then _alloc - offset else size' end

      if size' > 0 then
        from_cpointer(_ptr._offset(offset)._unsafe(), size', alloc)
      else
        create()
      end
    end

  fun iso chop(split_point: USize): (String iso^, String iso^) =>
    """
    Chops the string in half at the split point requested and returns both
    the left and right portions. The original string is trimmed in place and
    returned as the left portion. If the split point is larger than the
    string, the left portion is the original string and the right portion
    is a new empty string.
    Both strings are isolated and mutable, as they do not share memory.
    The operation does not allocate a new string pointer nor copy elements.
    """
    let start_ptr = cpointer(split_point)
    let size' = _size - _size.min(split_point)
    let alloc = _alloc - _size.min(split_point)

    trim_in_place(0, split_point)

    let right = recover
      if size' > 0 then
        from_cpointer(start_ptr._unsafe(), size', alloc)
      else
        create()
      end
    end

    (consume this, consume right)

  fun iso unchop(b: String iso): ((String iso^, String iso^) | String iso^) =>
    """
    Unchops two iso strings to return the original string they were chopped
    from. Both input strings are isolated and mutable and were originally
    chopped from a single string. This function checks that they are indeed two
    strings chopped from the same original string and can be unchopped before
    doing the unchopping and returning the unchopped string. If the two strings
    cannot be unchopped it returns both strings without modifying them.
    The operation does not allocate a new string pointer nor copy elements.
    """
    if _size == 0 then
      return consume b
    end

    if b.size() == 0 then
      return consume this
    end

    (let unchoppable, let a_left) =
      if (_size == _alloc) and (cpointer(_size) == b.cpointer()) then
        (true, true)
      elseif (b.size() == b.space()) and (b.cpointer(b.size()) == cpointer())
        then
        (true, false)
      else
        (false, false)
      end

    if not unchoppable then
      return (consume this, consume b)
    end

    if a_left then
      _alloc = _alloc + b._alloc
      _size = _size + b._size
      consume this
    else
      b._alloc = b._alloc + _alloc
      b._size = b._size + _size
      consume b
    end

  fun is_null_terminated(): Bool =>
    """
    Return true if the string is null-terminated and safe to pass to an FFI
    function that doesn't accept a size argument, expecting a null-terminator.
    This method checks that there is a null byte just after the final position
    of populated bytes in the string, but does not check for other null bytes
    which may be present earlier in the content of the string.
    If you need a null-terminated copy of this string, use the clone method.
    """
    (_alloc > 0) and (_alloc != _size) and (_ptr._apply(_size) == 0)

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
    let c = _ptr._apply(i)

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
        let c2 = _ptr._apply(i + 1)
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
        let c2 = _ptr._apply(i + 1)
        let c3 = _ptr._apply(i + 2)
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
        let c2 = _ptr._apply(i + 1)
        let c3 = _ptr._apply(i + 2)
        let c4 = _ptr._apply(i + 3)
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
    this(offset_to_index(offset))?

  fun ref update_offset(offset: ISize, value: U8): U8 ? =>
    """
    Changes a byte in the string, returning the previous byte at that offset.
    Raise an error if the offset is out of bounds.
    """
    this(offset_to_index(offset))? = value

  fun clone(): String iso^ =>
    """
    Returns a copy of the string. The resulting string is
    null-terminated even if the original is not.
    """
    let len = _size
    let str = recover String(len) end
    _ptr._copy_to(str._ptr._unsafe(), len)
    str._size = len
    str._set(len, 0)
    str

  fun repeat_str(num: USize = 1, sep: String = ""): String iso^ =>
    """
    Returns a copy of the string repeated `num` times with an optional
    separator added inbetween repeats.
    """
    var c = num
    var str = recover String((_size + sep.size()) * c) end

    while c > 0 do
      c = c - 1
      str = (consume str)._append(this)
      if (sep.size() > 0) and (c != 0) then
        str = (consume str)._append(sep)
      end
    end

    consume str

  fun mul(num: USize): String iso^ =>
    """
    Returns a copy of the string repeated `num` times.
    """
    repeat_str(num)

  fun find(s: String box, offset: ISize = 0, nth: USize = 0): ISize ? =>
    """
    Return the index of the n-th instance of s in the string starting from the
    beginning. Raise an error if there is no n-th occurrence of s or s is empty.
    """
    var i = offset_to_index(offset)
    var steps = nth + 1

    while i < _size do
      var j: USize = 0

      let same = while j < s._size do
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
    Return the index of n-th instance of `s` in the string starting from the
    end. The `offset` represents the highest index to included in the search.
    Raise an error if there is no n-th occurrence of `s` or `s` is empty.
    """
    var i = (offset_to_index(offset) + 1) - s._size

    var steps = nth + 1

    while i < _size do
      var j: USize = 0

      let same = while j < s._size do
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

  fun contains(s: String box, offset: ISize = 0, nth: USize = 0): Bool =>
    """
    Returns true if contains s as a substring, false otherwise.
    """
    var i = offset_to_index(offset)
    var steps = nth + 1

    while i < _size do
      var j: USize = 0

      let same = while j < s._size do
        if _ptr._apply(i + j) != s._ptr._apply(j) then
          break false
        end
        j = j + 1
        true
      else
        false
      end

      if same and ((steps = steps - 1) == 1) then
        return true
      end

      i = i + 1
    end
    false

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
      while k <= j do
        k = find(s, k)? + s.size().isize()
        i = i + 1
      end
    end

    i

  fun at(s: String box, offset: ISize = 0): Bool =>
    """
    Returns true if the substring s is present at the given offset.
    """
    let i = offset_to_index(offset)

    if (i + s.size()) <= _size then
      @memcmp(_ptr._offset(i), s._ptr, s._size) == 0
    else
      false
    end

  fun ref delete(offset: ISize, len: USize = 1) =>
    """
    Delete len bytes at the supplied offset, compacting the string in place.
    """
    let i = offset_to_index(offset)

    if i < _size then
      let n = len.min(_size - i)
      _size = _size - n
      _ptr._offset(i)._delete(n, _size - i)
      _set(_size, 0)
    end

  fun substring(from: ISize, to: ISize = ISize.max_value()): String iso^ =>
    """
    Returns a substring. Index range [`from` .. `to`) is half-open.
    Returns an empty string if nothing is in the range.

    Note that this operation allocates a new string to be returned. For
    similar operations that don't allocate a new string, see `trim` and
    `trim_in_place`.
    """
    let start = offset_to_index(from)
    let finish = offset_to_index(to).min(_size)

    if (start < _size) and (start < finish) then
      let len = finish - start
      let str = recover String(len) end
      _ptr._offset(start)._copy_to(str._ptr._unsafe(), len)
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
    let s = clone()
    s.lower_in_place()
    s

  fun ref lower_in_place() =>
    """
    Transforms the string to lower case. Currently only knows ASCII case.
    """
    var i: USize = 0

    while i < _size do
      let c = _ptr._apply(i)

      if (c >= 0x41) and (c <= 0x5A) then
        _set(i, c + 0x20)
      end

      i = i + 1
    end

  fun upper(): String iso^ =>
    """
    Returns an upper case version of the string. Currently only knows ASCII
    case.
    """
    let s = clone()
    s.upper_in_place()
    s

  fun ref upper_in_place() =>
    """
    Transforms the string to upper case.
    """
    var i: USize = 0

    while i < _size do
      let c = _ptr._apply(i)

      if (c >= 0x61) and (c <= 0x7A) then
        _set(i, c - 0x20)
      end

      i = i + 1
    end

  fun reverse(): String iso^ =>
    """
    Returns a reversed version of the string.
    """
    let s = clone()
    s.reverse_in_place()
    s

  fun ref reverse_in_place() =>
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

  fun ref push(value: U8) =>
    """
    Add a byte to the end of the string.
    """
    reserve(_size + 1)
    _set(_size, value)
    _size = _size + 1
    _set(_size, 0)

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

  fun ref unshift(value: U8) =>
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

  fun ref append(seq: ReadSeq[U8], offset: USize = 0, len: USize = -1) =>
    """
    Append the elements from a sequence, starting from the given offset.
    """
    if offset >= seq.size() then
      return
    end

    let copy_len = len.min(seq.size() - offset)
    reserve(_size + copy_len)

    match seq
    | let s: (String box | Array[U8] box) =>
      s._copy_to(_ptr, copy_len, offset, _size)
      _size = _size + copy_len
      _set(_size, 0)
    else
      let cap = copy_len + offset
      var i = offset

      try
        while i < cap do
          push(seq(i)?)
          i = i + 1
        end
      end
    end

  fun ref concat(iter: Iterator[U8], offset: USize = 0, len: USize = -1) =>
    """
    Add len iterated bytes to the end of the string, starting from the given
    offset.
    """
    try
      var n = USize(0)

      while n < offset do
        if iter.has_next() then
          iter.next()?
        else
          return
        end

        n = n + 1
      end

      n = 0

      while n < len do
        if iter.has_next() then
          push(iter.next()?)
        else
          return
        end

        n = n + 1
      end
    end

  fun ref clear() =>
    """
    Truncate the string to zero length.
    """
    _set(0, 0)
    _size = 0

  fun insert(offset: ISize, that: String): String iso^ =>
    """
    Returns a version of the string with the given string inserted at the given
    offset.
    """
    let s = clone()
    s.insert_in_place(offset, that)
    s

  fun ref insert_in_place(offset: ISize, that: String box) =>
    """
    Inserts the given string at the given offset. Appends the string if the
    offset is out of bounds.
    """
    reserve(_size + that._size)
    let index = offset_to_index(offset).min(_size)
    @memmove(_ptr.usize() + index + that._size,
      _ptr.usize() + index, _size - index)
    that._ptr._copy_to(_ptr._offset(index), that._size)
    _size = _size + that._size
    _set(_size, 0)

  fun ref insert_byte(offset: ISize, value: U8) =>
    """
    Inserts a byte at the given offset. Appends if the offset is out of bounds.
    """
    reserve(_size + 1)
    let index = offset_to_index(offset).min(_size)
    @memmove(_ptr.usize() + index + 1, _ptr.usize() + index,
      _size - index)
    _set(index, value)
    _size = _size + 1
    _set(_size, 0)

  fun cut(from: ISize, to: ISize = ISize.max_value()): String iso^ =>
    """
    Returns a version of the string with the given range deleted.
    Index range [`from` .. `to`) is half-open.
    """
    let s = clone()
    s.cut_in_place(from, to)
    s

  fun ref cut_in_place(from: ISize, to: ISize = ISize.max_value()) =>
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

  fun ref remove(s: String box): USize =>
    """
    Remove all instances of s from the string. Returns the count of removed
    instances.
    """
    var i: ISize = 0
    var n: USize = 0

    try
      while true do
        i = find(s, i)?
        cut_in_place(i, i + s.size().isize())
        n = n + 1
      end
    end
    n

  fun ref replace(from: String box, to: String box, n: USize = 0): USize =>
    """
    Replace up to n occurrences of `from` in `this` with `to`. If n is 0, all
    occurrences will be replaced. Returns the count of replaced occurrences.
    """
    let from_len = from.size().isize()
    let to_len = to.size().isize()
    var offset = ISize(0)
    var occur = USize(0)

    try
      while true do
        offset = find(from, offset)?
        cut_in_place(offset, offset + from_len)
        insert_in_place(offset, to)
        offset = offset + to_len
        occur = occur + 1

        if (n > 0) and (occur >= n) then
          break
        end
      end
    end
    occur

  fun split_by(
    delim: String,
    n: USize = USize.max_value())
    : Array[String] iso^
  =>
    """
    Split the string into an array of strings that are delimited by `delim` in
    the original string. If `n > 0`, then the split count is limited to n.

    Example:

    ```pony
    let original: String = "<b><span>Hello!</span></b>"
    let delimiter: String = "><"
    let split_array: Array[String] = original.split_by(delimiter)
    env.out.print("OUTPUT:")
    for value in split_array.values() do
      env.out.print(value)
    end

    // OUTPUT:
    // <b
    // span>Hello!</span
    // b>
    ```

    Adjacent delimiters result in a zero length entry in the array. For
    example, `"1CutCut2".split_by("Cut") => ["1", "", "2"]`.

    An empty delimiter results in an array that contains a single element equal
    to the whole string.

    If you want to split the string with each individual character of `delim`,
    use [`split`](#split).
    """
    let delim_size = ISize.from[USize](delim.size())
    let total_size = ISize.from[USize](size())

    let result = recover Array[String] end
    var current = ISize(0)

    while ((result.size() + 1) < n) and (current < total_size) do
      try
        let delim_start = find(delim where offset = current)?
        result.push(substring(current, delim_start))
        current = delim_start + delim_size
      else break end
    end
    result.push(substring(current))
    consume result

  fun split(delim: String = " \t\v\f\r\n", n: USize = 0): Array[String] iso^ =>
    """
    Split the string into an array of strings with any character in the
    delimiter string. By default, the string is split with whitespace
    characters. If `n > 0`, then the split count is limited to n.

    Example:

    ```pony
    let original: String = "name,job;department"
    let delimiter: String = ".,;"
    let split_array: Array[String] = original.split(delimiter)
    env.out.print("OUTPUT:")
    for value in split_array.values() do
      env.out.print(value)
    end

    // OUTPUT:
    // name
    // job
    // department
    ```

    Adjacent delimiters result in a zero length entry in the array. For
    example, `"1,,2".split(",") => ["1", "", "2"]`.

    If you want to split the string with the entire delimiter string `delim`,
    use [`split_by`](#split_by).
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
          (let c, let len) = utf32(i.isize())?

          if chars.contains(c) then
            // If we find a delimiter, add the current string to the array.
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

  fun ref strip(s: String box = " \t\v\f\r\n") =>
    """
    Remove all leading and trailing characters from the string that are in s.
    """
    this .> lstrip(s) .> rstrip(s)

  fun ref rstrip(s: String box = " \t\v\f\r\n") =>
    """
    Remove all trailing characters within the string that are in s. By default,
    trailing whitespace is removed.
    """
    if _size > 0 then
      let chars = Array[U32](s.size())
      var i = _size - 1
      var truncate_at = _size

      for rune in s.runes() do
        chars.push(rune)
      end

      repeat
        try
          match utf32(i.isize())?
          | (0xFFFD, 1) => None
          | (let c: U32, _) =>
            if not chars.contains(c) then
              break
            end
	    truncate_at = i
          end
        else
          break
        end
      until (i = i - 1) == 0 end

      truncate(truncate_at)
    end

  fun ref lstrip(s: String box = " \t\v\f\r\n") =>
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
          (let c, let len) = utf32(i.isize())?
          if not chars.contains(c) then
            break
          end
          i = i + len.usize()
        else
          break
        end
      end

      if i > 0 then
        delete(0, i)
      end
    end

  fun iso _append(s: String box): String iso^ =>
    let len = _size + s._size
    reserve(len)
    if s.is_null_terminated() then
      s._copy_to(_ptr._unsafe(), s._size + 1, 0, _size)
    else
      s._copy_to(_ptr._unsafe(), s._size, 0, _size)
    end
    _size = len
    consume this

  fun add(that: String box): String iso^ =>
    """
    Return a string that is a concatenation of this and that.
    """
    let len = _size + that._size
    let s = recover String(len) end
    (consume s)._append(this)._append(that)

  fun join(data: Iterator[Stringable]): String iso^ =>
    """
    Return a string that is a concatenation of the strings in data, using this
    as a separator.
    """
    var buf = recover String end
    var first = true
    for v in data do
      if first then
        first = false
      else
        buf = (consume buf)._append(this)
      end
      buf.append(v.string())
    end
    buf

  fun compare(that: String box): Compare =>
    """
    Lexically compare two strings.
    """
    compare_sub(that, _size.max(that._size))

  fun compare_sub(
    that: String box,
    n: USize,
    offset: ISize = 0,
    that_offset: ISize = 0,
    ignore_case: Bool = false)
    : Compare
  =>
    """
    Lexically compare at most `n` bytes of the substring of `this` starting at
    `offset` with the substring of `that` starting at `that_offset`. The
    comparison is case sensitive unless `ignore_case` is `true`.

    If the substring of `this` is a proper prefix of the substring of `that`,
    then `this` is `Less` than `that`. Likewise, if `that` is a proper prefix of
    `this`, then `this` is `Greater` than `that`.

    Both `offset` and `that_offset` can be negative, in which case the offsets
    are computed from the end of the string.

    If `n + offset` is greater than the length of `this`, or `n + that_offset`
    is greater than the length of `that`, then the number of positions compared
    will be reduced to the length of the longest substring.

    Needs to be made UTF-8 safe.
    """
    var j: USize = offset_to_index(offset)
    var k: USize = that.offset_to_index(that_offset)
    var i = n.min((_size - j).max(that._size - k))

    while i > 0 do
      // this and that are equal up to this point
      if j >= _size then
        // this is shorter
        return Less
      elseif k >= that._size then
        // that is shorter
        return Greater
      end

      let c1 = _ptr._apply(j)
      let c2 = that._ptr._apply(k)
      if
        not ((c1 == c2) or
          (ignore_case and ((c1 or 0x20) == (c2 or 0x20)) and
            ((c1 or 0x20) >= 'a') and ((c1 or 0x20) <= 'z')))
      then
        // this and that differ here
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

  fun bool(): Bool ? =>
    match lower()
    | "true" => true
    | "false" => false
    else
      error
    end

  fun i8(base: U8 = 0): I8 ? => _to_int[I8](base)?
  fun i16(base: U8 = 0): I16 ? => _to_int[I16](base)?
  fun i32(base: U8 = 0): I32 ? => _to_int[I32](base)?
  fun i64(base: U8 = 0): I64 ? => _to_int[I64](base)?
  fun i128(base: U8 = 0): I128 ? => _to_int[I128](base)?
  fun ilong(base: U8 = 0): ILong ? => _to_int[ILong](base)?
  fun isize(base: U8 = 0): ISize ? => _to_int[ISize](base)?
  fun u8(base: U8 = 0): U8 ? => _to_int[U8](base)?
  fun u16(base: U8 = 0): U16 ? => _to_int[U16](base)?
  fun u32(base: U8 = 0): U32 ? => _to_int[U32](base)?
  fun u64(base: U8 = 0): U64 ? => _to_int[U64](base)?
  fun u128(base: U8 = 0): U128 ? => _to_int[U128](base)?
  fun ulong(base: U8 = 0): ULong ? => _to_int[ULong](base)?
  fun usize(base: U8 = 0): USize ? => _to_int[USize](base)?

  fun _to_int[A: ((Signed | Unsigned) & Integer[A] val)](base: U8): A ? =>
    """
    Convert the *whole* string to the specified type.
    If there are any other characters in the string, or the integer found is
    out of range for the target type then an error is thrown.
    """
    (let v, let d) = read_int[A](0, base)?
    // Check the whole string is used
    if (d == 0) or (d.usize() != _size) then error end
    v

  fun read_int[A: ((Signed | Unsigned) & Integer[A] val)](
    offset: ISize = 0,
    base: U8 = 0)
    : (A, USize /* chars used */) ?
  =>
    """
    Read an integer from the specified location in this string. The integer
    value read and the number of bytes consumed are reported.
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
      if A(-1) > A(0) then
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

      value = if minus then
        (value *? base') -? digit
      else
        (value *? base') +? digit
      end

      had_digit = true
      index = index + 1
    end

    // Check result
    if not had_digit then
      // No integer found
      return (0, 0)
    end

    // Success
    (value, index - start_index)

  fun _read_int_base[A: ((Signed | Unsigned) & Integer[A] val)](
    base: U8,
    index: USize)
    : (A, USize /* chars used */)
  =>
    """
    Determine the base of an integer starting at the specified index.
    If a non-0 base is given use that. If given base is 0 read the base
    specifying prefix, if any, to detect base 2 or 16.
    If no base is specified and no prefix is found default to decimal.
    Note that a leading 0 does NOT imply octal.
    Report the base found and the number of single-byte characters in
    the prefix.
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

  fun f32(offset: ISize = 0): F32 ? =>
    """
    Convert this string starting at the given offset
    to a 32-bit floating point number ([F32](builtin-F32.md)).

    This method errors if this string cannot be parsed to a float,
    if the result would over- or underflow,
    the offset exceeds the size of this string or
    there are leftover characters in the string after conversion.

    Examples:

    ```pony
    "1.5".f32()? == F32(1.5)
    "1.19208e-07".f32()? == F32(1.19208e-07)
    "NaN".f32()?.nan() == true
    ```
    """
    let index = offset_to_index(offset)
    if index < _size then
      let ptr = this.cstring()
      var endp: Pointer[U8] box = Pointer[U8]
      @pony_os_clear_errno()
      let res = @strtof(ptr.offset(index), addressof endp)
      let errno: I32 = @pony_os_errno()
      if (errno != 0) or (endp != ptr.offset(_size)) then
        error
      else
        res
      end
    else
      error
    end

  fun f64(offset: ISize = 0): F64 ? =>
    """
    Convert this string starting at the given offset
    to a 64-bit floating point number ([F64](builtin-F64.md)).

    This method errors if this string cannot be parsed to a float,
    if the result would over- or underflow,
    the offset exceeds the size of this string or
    there are leftover characters in the string after conversion.

    Examples:

    ```pony
    "1.5".f64()? == F64(1.5)
    "1.19208e-07".f64()? == F64(1.19208e-07)
    "Inf".f64()?.infinite() == true
    ```
    """
    let index = offset_to_index(offset)
    if index < _size then
      let ptr = this.cstring()
      var endp: Pointer[U8] box = Pointer[U8]
      @pony_os_clear_errno()
      let res = @strtod(ptr.offset(index), addressof endp)
      let errno: I32 = @pony_os_errno()
      if (errno != 0) or (endp != ptr.offset(_size)) then
        error
      else
        res
      end
    else
      error
    end

  fun hash(): USize =>
    @ponyint_hash_block(_ptr, _size)

  fun hash64(): U64 =>
    @ponyint_hash_block64(_ptr, _size)

  fun string(): String iso^ =>
    clone()

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
    _string(_i = _i + 1)?

class StringRunes is Iterator[U32]
  let _string: String box
  var _i: USize

  new create(string: String box) =>
    _string = string
    _i = 0

  fun has_next(): Bool =>
    _i < _string.size()

  fun ref next(): U32 ? =>
    (let rune, let len) = _string.utf32(_i.isize())?
    _i = _i + len.usize()
    rune

primitive _UTF32Encoder
  fun encode(value: U32): (USize, U8, U8, U8, U8) =>
    """
    Encode the code point into UTF-8. It returns a tuple with the size of the
    encoded data and then the data.
    """
    if value < 0x80 then
      (1, value.u8(), 0, 0, 0)
    elseif value < 0x800 then
      ( 2,
        ((value >> 6) or 0xC0).u8(),
        ((value and 0x3F) or 0x80).u8(),
        0,
        0
      )
    elseif value < 0xD800 then
      ( 3,
        ((value >> 12) or 0xE0).u8(),
        (((value >> 6) and 0x3F) or 0x80).u8(),
        ((value and 0x3F) or 0x80).u8(),
        0
      )
    elseif value < 0xE000 then
      // UTF-16 surrogate pairs are not allowed.
      (3, 0xEF, 0xBF, 0xBD, 0)
    elseif value < 0x10000 then
      ( 3,
        ((value >> 12) or 0xE0).u8(),
        (((value >> 6) and 0x3F) or 0x80).u8(),
        ((value and 0x3F) or 0x80).u8(),
        0
      )
    elseif value < 0x110000 then
      ( 4,
        ((value >> 18) or 0xF0).u8(),
        (((value >> 12) and 0x3F) or 0x80).u8(),
        (((value >> 6) and 0x3F) or 0x80).u8(),
        ((value and 0x3F) or 0x80).u8()
      )
    else
      // Code points beyond 0x10FFFF are not allowed.
      (3, 0xEF, 0xBF, 0xBD, 0)
    end
