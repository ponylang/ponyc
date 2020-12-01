use @memcmp[I32](dst: Pointer[U8] box, src: Pointer[U8] box, len: USize)
use @memmove[Pointer[None]](dst: Pointer[None], src: Pointer[None], len: USize)
use @strtof[F32](nptr: Pointer[U8] box, endptr: Pointer[Pointer[U8] box] ref)
use @strtod[F64](nptr: Pointer[U8] box, endptr: Pointer[Pointer[U8] box] ref)
use @pony_os_clear_errno[None]()
use @pony_os_errno[I32]()

class val String is (Seq[U32] & Comparable[String box] & Stringable)
  """
  A String is an ordered collection of unicode codepoints.

  Strings don't specify an encoding, and conversion of String to and from bytes always requires specifying
  an encoding or decoding.

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
      let first_w = str_new.find("w")
      // find first occurrence of letter "d"
      let first_d = str_new.find("d")

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

  new val from_array[D: StringDecoder = UTF8StringDecoder](data: Array[U8] val) =>
    """
    Create a string from an array, reusing the underlying data pointer
    if the provided decoder matches the encoding used internally by the
    string (UTF-8). If the decoder does not match, a new byte array is
    allocated. Any invalid bytes will be converted to the unicode replacement
    character U+FFFD
    """
    iftype D <: UTF8StringDecoder then
      try
        _validate_encoding(data, D)?
        _size = data.size()
        _alloc = data.space()
        _ptr = data.cpointer()._unsafe()
        return
      end
    end
    let utf8_encoded_bytes = recover _recode_byte_array(data, D) end
    _size = utf8_encoded_bytes.size()
    _alloc = utf8_encoded_bytes.space()
    _ptr = utf8_encoded_bytes.cpointer()._unsafe()

  new val from_codepoint_array(data: Array[U32] val, encoded_size_estimate: USize = 1) =>
    """
    Create a string from an array of unicode codepoints. In all cases, a
    new byte array is allocated.
    """
    _size = 0
    _alloc = (data.size() * encoded_size_estimate) + 1
    _ptr = Pointer[U8]._alloc(_alloc)
    _set(0, 0)
    for codepoint in data.values() do
      push(codepoint)
    end

  new iso from_iso_array[D: StringDecoder = UTF8StringDecoder](data: Array[U8] iso) =>
    """
    Create a string from an array, reusing the underlying data pointer
    if the provided decoder matches the encoding used internally by the
    string (UTF-8). If the decoder does not match, a new byte array is
    allocated. Any invalid bytes will be converted to the unicode replacement
    character U+FFFD
    """
    var validation_error: Bool = false
    var d3: Array[U8] iso = recover Array[U8](0) end
    iftype D <: UTF8StringDecoder then
      let d2 = recover
        let d1: Array[U8] ref = consume data
        try
          _validate_encoding(d1, D)?
        else
          validation_error = true
        end
        d1
      end
      if not validation_error then
        _size = d2.size()
        _alloc = d2.space()
        _ptr = (consume d2).cpointer()._unsafe()
        return
      else
        d3 = consume d2
      end
    else
      d3 = consume data
    end

    let utf8_encoded_bytes = recover _recode_byte_array(consume d3, D) end
    _size = utf8_encoded_bytes.size()
    _alloc = utf8_encoded_bytes.space()
    _ptr = utf8_encoded_bytes.cpointer()._unsafe()

    if _alloc > _size then
      _set(_size, 0)
    end

  new val from_iso_codepoint_array(data: Array[U32] val, encoded_size_estimate: USize = 1) =>
    """
    Create a string from an array of unicode codepoints. In all cases, a
    new byte array is allocated.
    """
    _size = 0
    _alloc = (data.size() * encoded_size_estimate) + 1
    _ptr = Pointer[U8]._alloc(_alloc)
    _set(0, 0)
    for codepoint in data.values() do
      push(codepoint)
    end

  new from_cpointer(str: Pointer[U8], len: USize, alloc: USize = 0) =>
    """
    Create a string from binary pointer data without making a
    copy. This must be done only with C-FFI functions that return
    pony_alloc'd character arrays. If a null pointer is given then an
    empty string is returned. The pointer data must be UTF-8 encoded
    unicode codepoints.
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
    Create a string from a pointer to a null-terminated cstring
    without making a copy. The data is not copied. This must be done
    only with C-FFI functions that return pony_alloc'd character
    arrays. The pointer is scanned for the first null byte, which will
    be interpreted as the null terminator. Note that the scan is
    unbounded; the pointed to data must be null-terminated within
    the allocated array to preserve memory safety. If a null pointer
    is given then an empty string is returned. The pointer data must
    be UTF-8 encoded unicode codepoints.
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

  new copy_cpointer(str: Pointer[U8] box, len: USize) =>
    """
    Create a string by copying a fixed number of bytes from a pointer.
    The pointer data must be UTF-8 encoded unicode codepoints.
    """
    if str.is_null() then
      _size = 0
      _alloc = 1
      _ptr = Pointer[U8]._alloc(_alloc)
      _set(0, 0)
    else
      _size = len
      _alloc = _size + 1
      _ptr = Pointer[U8]._alloc(_alloc)
      str._copy_to(_ptr, _alloc)
    end

  new copy_cstring(str: Pointer[U8] box) =>
    """
    Create a string by copying a null-terminated C string. Note that
    the scan is unbounded; the pointed to data must be null-terminated
    within the allocated array to preserve memory safety. If a null
    pointer is given then an empty string is returned. The pointer data
    must be UTF-8 encoded unicode codepoints.
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
      _ptr = Pointer[U8]._alloc(_alloc)
      str._copy_to(_ptr, _alloc)
    end

  new from_utf32(value: U32) =>
    """
    Create a string from a single UTF-32 code point.
    """
    let byte_array = Array[U8](4)
    UTF8StringEncoder._add_encoded_bytes(byte_array, UTF8StringEncoder.encode(value))

    _size = byte_array.size()
    _alloc = _size + 1
    _ptr = Pointer[U8]._alloc(_alloc)
    byte_array._copy_to(_ptr, _size)
    _set(_size, 0)

  fun ref push_utf32(value: U32) =>
    """
    Push a UTF-32 code point. This function is maintained for
    backard compatability. Use push() instead.
    """"
    push(value)

  fun box _copy_to(ptr: Pointer[U8] ref, copy_len: USize,
    from_offset: USize = 0, to_offset: USize = 0) =>
    """
    Copy copy_len characters from this to that at specified offsets.
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

  fun val array[E: StringEncoder val = UTF8StringEncoder](): Array[U8] val =>
    """
    Returns an Array[U8] that reuses the underlying data pointer if
    the provided Encoder matches the default system string encoding
    (UTF-8). If the encoder doss not match, a new byte array is
    allocated and returned.
    """
    recover
      var rtrn_array: Array[U8]
      iftype E <: UTF8StringEncoder then
        rtrn_array = Array[U8].from_cpointer(_ptr._unsafe(), _size, _alloc)
      else
        rtrn_array = Array[U8](_size)
        for c in values() do
            UTF8StringEncoder._add_encoded_bytes(rtrn_array, E.encode(c))
        end
      end
      rtrn_array
    end

  fun iso iso_array[E: StringEncoder val = UTF8StringEncoder](): Array[U8] iso^ =>
    """
    Returns an Array[U8] that reuses the underlying data pointer if
    the provided Encoder matches the default system string encoding
    (UTF-8). If the encoder doss not match, a new byte array is
    allocated and returned.
    """
    recover
      var rtrn_array: Array[U8]
      iftype E <: UTF8StringEncoder then
        rtrn_array = Array[U8].from_cpointer(_ptr._unsafe(), _size, _alloc)
      else
        rtrn_array = Array[U8](_size)
        for c in (consume this).values() do
          UTF8StringEncoder._add_encoded_bytes(rtrn_array, E.encode(c))
        end
      end
      rtrn_array
    end

  fun size(): USize =>
    """
    Returns the number of unicode codepoints in the string.
    """
    if _size == 0 then
      return 0
    end

    var i = USize(0)
    var n = USize(0)

    while i < _size do
      if (_ptr._apply(i) and 0xC0) != 0x80 then
        n = n + 1
      end

      i = i + 1
    end

    n

  fun codepoints(from: ISize = 0, to: ISize = ISize.max_value()): USize =>
    """
    Returns the number of unicode code points in the string between the two
    offsets. Index range [`from` .. `to`) is half-open.
    """
    if _size == 0 then
      return 0
    end

    var i = _offset_to_index(from)
    let j = _offset_to_index(to).min(_size)
    var n = USize(0)

    while i < j do
      if (_ptr._apply(i) and 0xC0) != 0x80 then
        n = n + 1
      end

      i = i + 1
    end

    n

  fun _byte_offset(offset: USize): USize =>
    """
    Returns the byte offset in the Pointer[U8] of a unicode code point in
    the string.
    """
    var i = USize(0)
    var n = USize(0)

    while (n <= offset) and (i < _size) do
      if (_ptr._apply(i) and 0xC0) != 0x80 then
        n = n + 1
      end

      if n <= offset then
        i = i + 1
      end
    end

    i

  fun byte_size(): USize =>
    """
    Returns the size of the string in encoded bytes.
    """
    _size

  fun space(): USize =>
    """
    Returns the space available for data, not including the null terminator.
    Space is measured in bytes, and space for bytes does not imply space for
    the same number of unicode characters
    """
    if is_null_terminated() then _alloc - 1 else _alloc end

  fun ref reserve(len: USize) =>
    """
    Reserve space for len bytes, and space for bytes does not imply space for
    the same number of unicode characters. An additional byte will be reserved
    for the null terminator.
    """
    if _alloc <= len then
      let max = len.max_value() - 1
      let min_alloc = len.min(max) + 1
      if min_alloc <= (max / 2) then
        _alloc = min_alloc.next_pow2()
      else
        _alloc = min_alloc.min(max)
      end
      _ptr = _ptr._realloc(_alloc)
    end

  fun ref compact() =>
    """
    Try to remove unused space, making it available for garbage collection. The
    request may be ignored.
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

  fun ref resize(len: USize) =>
    """
    Increase the size of a string to the give len in bytes. This is an
    unsafe operation, and should only be used when string's _ptr has
    been manipulated through a FFI call and the string size is known.
    """
    if len > _size then
      _size = len
      _set(_size, 0)
    end

  fun ref truncate(len: USize) =>
    """
    Truncates the string at the minimum of len and size. Ensures there is a
    null terminator. Does not check for null terminators inside the string.
    Truncate does not work with a len that is larger than the string size.
    """
    let byte_offset = _offset_to_index(len.isize())
    if byte_offset <= _size then
      _truncate(byte_offset)
    end

  fun ref _truncate(len: USize) =>
    """
    Truncates the string at the minimum of len and size. Ensures there is a
    null terminator. Does not check for null terminators inside the string.

    Note that memory is not freed by this operation.
    """
    _size = len.min(_size)
    if _size < _alloc then
      _set(_size, 0)
    end

  fun ref trim_in_place(from: USize = 0, to: USize = -1) =>
    """
    Trim the string to a portion of itself, covering `from` until `to`.
    Unlike slice, the operation does not allocate a new string nor copy
    elements.
    """
    var last: USize = 0
    let offset = _offset_to_index(from.isize())

    if (to > to.isize().max_value().usize()) then
      last = _size
    else
      if (offset < _size) and (to > from) then
        last = _offset_to_index((to - from).isize(), offset)
      else
        last = offset
      end
    end
    _trim_in_place(offset, last)

  fun ref _trim_in_place(from: USize, to: USize) =>

    let size' = to - from

    // use the new size' for alloc if we're not including the last used byte
    // from the original data and only include the extra allocated bytes if
    // we're including the last byte.
    _alloc = if to == _size then _alloc - from else size' end

    _size = size'

    // if _alloc == 0 then we've trimmed all the memory originally allocated.
    // if we do _ptr._offset, we will spill into memory not allocated/owned
    // by this string and could potentially cause a segfault if we cross
    // a pagemap boundary into a pagemap address that hasn't been allocated
    // yet when `reserve` is called next.
    if _alloc == 0 then
      _ptr = Pointer[U8]
    else
      _ptr = _ptr._offset(from)
    end

  fun val trim(from: USize = 0, to: USize = -1): String val =>
    """
    Return a shared portion of this string, covering `from` until `to`.
    Both the original and the new string are immutable, as they share memory.
    The operation does not allocate a new string pointer nor copy elements.
    """
    var last: USize = 0
    let offset = _offset_to_index(from.isize())
    if (to > to.isize().max_value().usize()) then
      last = _size
    else
      if (offset < _size) and (to > from) then
        last = _offset_to_index((to - from).isize(), offset)
      else
        last = offset
      end
    end

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
    let split_point_index = _offset_to_index(split_point.isize())
    let start_ptr = cpointer(split_point)
    let size' = _size - _size.min(split_point_index)
    let alloc = _alloc - _size.min(split_point_index)

    _trim_in_place(0, split_point_index)

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

    if b._size == 0 then
      return consume this
    end

    (let unchoppable, let a_left) =
      if (_size == _alloc) and (cpointer(_size) == b.cpointer()) then
        (true, true)
      elseif (b._size == b.space()) and (b.cpointer(b._size) == cpointer())
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

  fun _codepoint(byte_offset: USize): (U32, U8) ? =>
    """
    Return a UTF32 representation of the character at the given offset and the
    number of bytes needed to encode that character. If the offset does not
    point to the beginning of a valid UTF8 encoding, return 0xFFFD (the unicode
    replacement character) and a length of one. Raise an error if the offset is
    out of bounds.
    """
    let err: (U32, U8) = (0xFFFD, 1)

    if byte_offset >= _size then error end
    let c = _ptr._apply(byte_offset)

    if c < 0x80 then
      // 1-byte
      (c.u32(), 1)
    elseif c < 0xC2 then
      // Stray continuation.
      err
    elseif c < 0xE0 then
      // 2-byte
      if (byte_offset + 1) >= _size then
        // Not enough bytes.
        err
      else
        let c2 = _ptr._apply(byte_offset + 1)
        if (c2 and 0xC0) != 0x80 then
          // Not a continuation byte.
          err
        else
          (((c.u32() << 6) + c2.u32()) - 0x3080, 2)
        end
      end
    elseif c < 0xF0 then
      // 3-byte.
      if (byte_offset + 2) >= _size then
        // Not enough bytes.
        err
      else
        let c2 = _ptr._apply(byte_offset + 1)
        let c3 = _ptr._apply(byte_offset + 2)
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
      if (byte_offset + 3) >= _size then
        // Not enough bytes.
        err
      else
        let c2 = _ptr._apply(byte_offset + 1)
        let c3 = _ptr._apply(byte_offset + 2)
        let c4 = _ptr._apply(byte_offset + 3)
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

  fun _next_char(index: USize): USize =>
    var i = index + 1
    while (i < _size) and ((_ptr._apply(i) and 0xC0) == 0x80) do
      i = i + 1
    end
    i

  fun _previous_char(index: USize): USize =>
    var i = index - 1
    while (i > 0) and ((_ptr._apply(i) and 0xC0) == 0x80) do
      i = i - 1
    end
    i

  fun apply(i: USize): U32 ? =>
    """
    Returns the i-th unicode codepoint. Raise an error if the index is out of bounds.
    """
    (let codepoint, let sz) = _codepoint(_byte_offset(i))?
    codepoint

  fun ref update(i: USize, value: U32): U32 ? =>
    """
    Change the i-th character. Raise an error if the index is out of bounds.
    """
    if i < _size then
      (let c, let sz) = _codepoint(i)?
      _cut_in_place(i, i+sz.usize())
      _insert_in_place(i, String.from_utf32(value))
      c
    else
      error
    end

  fun at_offset(offset: ISize): U32 ? =>
    """
    Returns the character at the given offset. Raise an error if the offset
    is out of bounds.
    """
    this(_offset_to_index(offset))?

  fun ref update_offset(offset: ISize, value: U32): U32 ? =>
    """
    Changes a character in the string, returning the previous byte at
    that offset. Raise an error if the offset is out of bounds.
    """
    update(_offset_to_index(offset), value)?

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
    var str = recover String((_size + sep._size) * c) end

    while c > 0 do
      c = c - 1
      str = (consume str)._append(this)
      if (sep._size > 0) and (c != 0) then
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
    Return the index (characters) of the n-th instance of s in the string
    starting from the offset (characters). Raise an error if there is no n-th
    occurrence of s or s is empty.
    """
    let index = _offset_to_index(offset)
    if index < _size then
      (let offset', _) = _find(s, _offset_to_index(offset), nth)?
      return offset + offset'
    end
    error

  fun _find(s: String box, index: USize, nth: USize): (ISize, USize) ? =>
    """
    Return a tuple containing the number of characters from the index and the
    byte index of the n-th instance of s in the string starting from the
    given index (bytes). Raise an error if there is no n-th occurrence of s or s
    is empty.
    """
    var i_byte = index
    var i_char = ISize(0)
    var steps = nth + 1

    while i_byte < _size do
      var j_byte: USize = 0

      let same = while j_byte < s._size do
        (let this_char, let this_sz) = _codepoint(i_byte + j_byte)?
        (let that_char, let that_sz) = s._codepoint(j_byte)?
        if this_char != that_char then
          break false
        end
        j_byte = j_byte + this_sz.usize()
        true
      else
        false
      end

      if same and ((steps = steps - 1) == 1) then
        return (i_char, i_byte - index)
      end

      i_byte = _next_char(i_byte)
      i_char = i_char + 1
    end
    error

  fun rfind(s: String box, offset: ISize = -1, nth: USize = 0): ISize ? =>
    """
    Return the index of n-th instance of `s` in the string starting from the
    end. The `offset` represents the highest index to included in the search.
    Raise an error if there is no n-th occurrence of `s` or `s` is empty.
    """
    var index = _offset_to_index(offset)
    if (index >= _size) or (s._size > index) then
      error
    end

    var i_byte = (index + 1) - s._size
    var i_char = if offset < 0 then size().isize() + (offset + 1) else offset + 1 end
    i_char = i_char - s.size().isize()

    var steps = nth + 1

    while i_byte < _size do
      var j_byte: USize = 0

      let same = while j_byte < s._size do
        (let this_char, let this_sz) = _codepoint(i_byte + j_byte)?
        (let that_char, let that_sz) = s._codepoint(j_byte)?
        if this_char != that_char then
          break false
        end
        j_byte = j_byte + this_sz.usize()
        true
      else
        false
      end

      if same and ((steps = steps - 1) == 1) then
        return i_char
      end

      i_byte = _previous_char(i_byte)
      i_char = i_char - 1
    end
    error

  fun contains(s: String box, offset: ISize = 0, nth: USize = 0): Bool =>
    """
    Returns true if contains s as a substring, false otherwise.
    """
    var i_byte = _offset_to_index(offset)
    var steps = nth + 1

    while (i_byte + s._size) <= _size do
      var j_byte: USize = 0

      let same = while j_byte < s._size do
        try
          (let this_char, let this_sz) = _codepoint(i_byte + j_byte)?
          (let that_char, let that_sz) = s._codepoint(j_byte)?
          if this_char != that_char then
            break false
          end
          j_byte = j_byte + this_sz.usize()
        else
          return false // this should never happen
        end
        true
      else
        false
      end

      if same and ((steps = steps - 1) == 1) then
        return true
      end

      i_byte = _next_char(i_byte)
    end
    false

  fun count(s: String box, offset: ISize = 0): USize =>
    """
    Counts the non-overlapping occurrences of s in the string.
    """
    let j_byte = _size - s._size

    if j_byte < 0 then
      return 0
    elseif (j_byte == 0) and (this == s) then
      return 1
    end

    var i: USize = 0
    var k_byte = _offset_to_index(offset)

    try
      while k_byte <= j_byte do
        (_, let k_byte') = _find(s, k_byte, 0)?
        k_byte = k_byte + k_byte' + s._size
        i = i + 1
      end
    end

    i

  fun at(s: String box, offset: ISize = 0): Bool =>
    """
    Returns true if the substring s is present at the given offset.
    """
    let i_byte = _offset_to_index(offset)

    if (i_byte + s._size) <= _size then
      @memcmp(_ptr._offset(i_byte), s._ptr, s._size) == 0
    else
      false
    end

  fun ref delete(offset: ISize, len: USize = 1) =>
    """
    Delete len characters at the supplied offset, compacting the string
    in place.
    """
    let byte_offset = _offset_to_index(offset)

    var len_counter = len
    var byte_len = USize(0)
    try
      while (len_counter > 0) and ((byte_offset + byte_len) < _size) do
        (_, let sz) = _codepoint(byte_offset + byte_len) ?
        len_counter = len_counter - 1
        byte_len = byte_len + sz.usize()
      end
    else
      return // Assuming that this condition will never happen
    end

    _delete(byte_offset, byte_len)

  fun ref _delete(offset: USize, len: USize = 1) =>
    """
    Delete len bytes at the supplied offset, compacting the string in place.
    """
    let i = offset

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
    let start = _offset_to_index(from)
    let finish = _offset_to_index(to).min(_size)
    _substring(start, finish)

  fun _substring(start: USize, finish: USize): String iso^ =>
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
    Returns a lower case version of the string. Currently only knows ASCII
    case.
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

      if (c and 0x80) == 0 then
          if (c >= 0x41) and (c <= 0x5A) then
            _set(i, c + 0x20)
          end
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
    Transforms the string to upper case. Currently only knows ASCII case.
    """
    var i: USize = 0

    while i < _size do
      let c = _ptr._apply(i)

      if (c and 0x80) == 0 then
        if (c >= 0x61) and (c <= 0x7A) then
          _set(i, c - 0x20)
        end
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
    Reverses the character order in the string.
    """
    if _size > 1 then
      var i: USize = 0
      var j = _size
      reserve(_size + 1)

      while i < _size do
        try
          (let c, let sz) = _codepoint(0)?
          j = j - sz.usize()
          @memmove(_ptr.usize(), _ptr.usize() + sz.usize(), j)
          let s = String.from_utf32(c)
          s._ptr._copy_to(_ptr._offset(j), s._size)
          i = i + sz.usize()
        else
          return
        end
      end
    end

  fun ref push(value: U32) =>
    """
    Push a character onto the end of the string.
    """
    let encoded = UTF8StringEncoder.encode(value)
    let i = _size
    _size = _size + encoded._1
    reserve(_size)
    _set(i, (encoded._2 and 0xFF).u8())
    if encoded._1 > 1 then
      _set(i + 1, ((encoded._2 >> 8) and 0xFF).u8())
      if encoded._1 > 2 then
        _set(i + 2, ((encoded._2 >> 16) and 0xFF).u8())
        if encoded._1 > 3 then
          _set(i + 3, ((encoded._2 >> 24) and 0xFF).u8())
        end
      end
    end
    _set(_size, 0)

  fun ref pop(): U32 ? =>
    """
    Removes a character from the end of the string.
    """
    if _size > 0 then
      let i = _offset_to_index(-1)
      (let c, let sz) = _codepoint(i)?
      _delete(_size - sz.usize(), sz.usize())
      c
    else
      error
    end

  fun ref unshift(value: U32) =>
    """
    Adds a character to the beginning of the string.
    """
    if value != 0 then
      _insert_in_place(0, String.from_utf32(value))
    else
      _set(0, 0)
      _size = 0
    end

  fun ref shift(): U32 ? =>
    """
    Removes a character from the beginning of the string.
    """
    if _size > 0 then
      (let c, let sz) = _codepoint(0)?
      _cut_in_place(0, sz.usize())
      c
    else
      error
    end

  fun ref append(seq: ReadSeq[U32], offset: USize = 0, len: USize = -1) =>
    """
    Append the elements from a sequence, starting from the given offset.
    """
    if offset > 0 then
      if offset >= seq.size() then
        return
      end
    end

    match seq
    | let s: (String box) =>
      let index = if offset > 0 then _offset_to_index(offset.isize()) else 0 end
      let copy_len = s._size - index
      reserve(_size + copy_len)
      s._copy_to(_ptr, copy_len, index, _size)
      _size = _size + copy_len
      _set(_size, 0)
    else
      let copy_len = len.min(seq.size() - offset)
      reserve(_size + (copy_len * 4))
      let cap = copy_len + offset
      var i = USize(0)

      try
        let iterator: Iterator[U32] = seq.values()
        while (i < cap) and (iterator.has_next()) do
          let c = iterator.next()?
          if i >= offset then
            push(c)
          end
          i = i + 1
        end
      end
    end

  fun ref concat(iter: Iterator[U32], offset: USize = 0, len: USize = -1) =>
    """
    Add len iterated characters to the end of the string, starting from the given
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

  fun ref concat_bytes[D: StringDecoder = UTF8StringDecoder](iter: Iterator[U8], offset: USize = 0, len: USize = -1) =>
    """
    Add all iterated bytes to the end of the string converting bytes to codepoints
    using the provided Decoder.
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

      _process_byte_array(_LimittedIterator[U8](iter, len),
                          D,
                          {ref(codepoint: U32)(str = this) =>
                            str.push(codepoint)
                          })
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
    let index = _offset_to_index(offset)
    _insert_in_place(index, that)

  fun ref _insert_in_place(index: USize, that: String box) =>
    reserve(_size + that._size)
    @memmove(_ptr.usize() + index + that._size,
      _ptr.usize() + index, _size - index)
    that._ptr._copy_to(_ptr._offset(index), that._size)
    _size = _size + that._size
    _set(_size, 0)

  fun ref insert_utf32(offset: ISize, value: U32) =>
    """
    Inserts a character at the given offset. The value must contain
    the UTF-8 encoded bytes of the character. Appends if the offset
    is out of bounds.
    """

    insert_in_place(offset, String.from_utf32(value))

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
    let from' = _offset_to_index(from)
    let to' = _offset_to_index(to)
    _cut_in_place(from', to')

  fun ref _cut_in_place(from: USize, to: USize) =>
    """
    Cuts the given range out of the string.
    Index range [`from` .. `to`) is half-open.
    """
    let start = from
    let finish = to

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
    var i: USize = 0
    var n: USize = 0

    try
      while true do
        (_, let i') = _find(s, i, 0)?
        i = i + i'
        _cut_in_place(i, i + s._size)
        n = n + 1
      end
    end
    n

  fun ref replace(from: String box, to: String box, n: USize = 0): USize =>
    """
    Replace up to n occurrences of `from` in `this` with `to`. If n is 0, all
    occurrences will be replaced. Returns the count of replaced occurrences.
    """
    let from_len = from._size
    let to_len = to._size
    var offset = USize(0)
    var occur = USize(0)

    try
      while true do
        (_, let offset') = _find(from, offset, 0)?
        offset = offset + offset'
        _cut_in_place(offset, offset + from_len)
        _insert_in_place(offset, to)
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
    let result = recover Array[String] end
    var current = USize(0)

    while ((result.size() + 1) < n) and (current < _size) do
      try
        (_, let delim_start) = _find(delim, current, 0)?
        result.push(_substring(current, current + delim_start))
        current = current + (delim_start + delim._size)
      else break end
    end
    result.push(_substring(current, _size))
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

      for rune in delim.values() do
        chars.push(rune)
      end

      var cur = recover String end
      var i = USize(0)
      var occur = USize(0)

      try
        while i < _size do
          (let c, let len) = _codepoint(i)?

          if chars.contains(c) then
            // If we find a delimiter, add the current string to the array.
            occur = occur + 1

            if (n > 0) and (occur >= n) then
              break
            end

            result.push(cur = recover String end)
          else
            // Add bytes to the current string.
            cur.push(c)
          end

          i = i + len.usize()
        end

        // Add all remaining bytes to the current string.
        while i < _size do
          (let c, let len) = _codepoint(i)?
          cur.push(c)
          i = i + len.usize()
        end
        result.push(consume cur)
      end
    end

    consume result

  fun ref strip(s: String box = " \t\v\f\r\n") =>
    """
    Remove all leading and trailing characters from the string that are in s.
    """
      var i = _size - 1
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

      for rune in s.values() do
        chars.push(rune)
      end

      repeat
        try
          match _codepoint(i)?
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

      _truncate(truncate_at)
    end

  fun ref lstrip(s: String box = " \t\v\f\r\n") =>
    """
    Remove all leading characters within the string that are in s. By default,
    leading whitespace is removed.
    """
    if _size > 0 then
      let chars = Array[U32](s.size())
      var i = USize(0)

      for rune in s.values() do
        chars.push(rune)
      end

      while i < _size do
        try
          (let c, let len) = _codepoint(i)?
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

  fun add(that: String box): String =>
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
    var j: USize = _offset_to_index(offset)
    var k: USize = that._offset_to_index(that_offset)
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

      try
        (let c1, let this_sz) = _codepoint(j)?
        (let c2, let that_sz) = that._codepoint(k)?
        if
          not ((c1 == c2) or
            (ignore_case and ((c1 or 0x20) == (c2 or 0x20)) and
              ((c1 or 0x20) >= 'a') and ((c1 or 0x20) <= 'z')))
        then
          // this and that differ here
          return if c1.i32() > c2.i32() then Greater else Less end
        end

        j = j + this_sz.usize()
        k = k + that_sz.usize()
        i = i - this_sz.usize()
      else
        return Equal // This error should never happen
      end
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

    try
      while i < len do
        (let c1, let this_sz) = _codepoint(i)?
        (let c2, let that_sz) = that._codepoint(i)?

        if c1 < c2 then
          return true
        elseif c1 > c2 then
          return false
        end
        i = i + this_sz.usize()
      end
      _size < that._size
    else
      return false // This should never happen
    end

  fun le(that: String box): Bool =>
    """
    Returns true if this is lexically less than or equal to that. Needs to be
    made UTF-8 safe.
    """
    let len = _size.min(that._size)
    var i: USize = 0

    try
      while i < len do
        (let c1, let this_sz) = _codepoint(i)?
        (let c2, let that_sz) = that._codepoint(i)?

        if c1 < c2 then
          return true
        elseif c1 > c2 then
          return false
        end
        i = i + this_sz.usize()
      end
      _size <= that._size
    else
      return false // This should never happen
    end

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
    : (A, USize /* bytes used */) ?
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
    let start_index = _offset_to_index(offset)
    var index = start_index
    var value: A = 0
    var had_digit = false

    // Check for leading minus
    let minus = (index < _size) and (_codepoint(index)?._1 == '-')
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
      (let c, let sz) = _codepoint(index)?
      let char: A = A(0).from[U32](c)
      if char == '_' then
        index = index + sz.usize()
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
      index = index + sz.usize()
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

  fun _offset_to_index(offset: ISize, start: USize = 0): USize =>
    let limit: USize = _size
    var inc: ISize = 1
    var n = ISize(0)
    var i = start.min(_size)
    if offset < 0 then
      inc = -1
      if start == 0 then
        i = _size - 1
      else
        i = start - 1
      end
    end

    while (((inc > 0) and (i < limit) and (n <= offset)) or
           ((inc < 0) and (i >= 0) and (n > offset))) do
      if (_ptr._apply(i.usize()) and 0xC0) != 0x80 then
        n = n + inc
      end

      if ((inc > 0) and (n <= offset)) or ((inc < 0) and (n > offset)) then
        if inc < 0 then
          i = i - 1
        else
          i = i + 1
        end
      end
    end

    if (i < 0) or (i == limit) then
      return limit
    end
    i

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
    let index = _offset_to_index(offset)
    if index < _size then
      @pony_os_clear_errno()
      var endp: Pointer[U8] box = Pointer[U8]
      let res = @strtof(_ptr._offset(index), addressof endp)
      let errno: I32 = @pony_os_errno()
      if (errno != 0) or (endp != _ptr._offset(_size)) then
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
    let index = _offset_to_index(offset)
    if index < _size then
      @pony_os_clear_errno()
      var endp: Pointer[U8] box = Pointer[U8]
      let res = @strtod(_ptr._offset(index), addressof endp)
      let errno: I32 = @pony_os_errno()
      if (errno != 0) or (endp != _ptr._offset(_size)) then
        error
      else
        res
      end
    else
      error
    end

  fun hash(): USize =>
    @ponyint_hash_block[USize](_ptr, _size)

  fun hash64(): U64 =>
    @ponyint_hash_block64[U64](_ptr, _size)

  fun string(): String iso^ =>
    clone()

  fun runes(): StringRunes^ =>
    """
    Return an iterator over the codepoints in the string.
    """
    StringRunes(this)

  fun values(): StringRunes^ =>
    """
    Return an iterator over the codepoint in the string.
    """
    StringRunes(this)

  fun bytes[E: StringEncoder val = UTF8StringEncoder](): Iterator[U8] =>
    StringBytes(this, E)

  fun _byte(i: USize): U8 =>
    _ptr._apply(i)

  fun ref _set(i: USize, value: U8): U8 =>
    """
    Unsafe update, used internally.
    """
    _ptr._update(i, value)

  fun tag _validate_encoding(data: Array[U8] box, decoder: StringDecoder) ? =>
    let byte_consumer = {(codepoint: U32) => None} ref
    if not _process_byte_array(data.values(), decoder, byte_consumer) then
      error
    end

  fun tag _recode_byte_array(data: Array[U8] box, decoder: StringDecoder val): Array[U8] =>
    let utf8_encoded_bytes = Array[U8](data.size())
    let byte_consumer = {ref(codepoint: U32)(utf8_encoded_bytes) =>
      UTF8StringEncoder._add_encoded_bytes(utf8_encoded_bytes, UTF8StringEncoder.encode(codepoint))
    }
    _process_byte_array(data.values(), decoder, byte_consumer)
    utf8_encoded_bytes

  fun tag _process_byte_array(data: Iterator[U8] ref,
                              decoder: StringDecoder val,
                              byte_consumer: {ref(U32)} ref) : Bool =>
    var decode_error: Bool = false
    let v_bytes = StringDecoderBytes.create()
    for b in data do
      v_bytes.pushByte(b)

      if v_bytes.bytes_loaded() == 4 then
        let decode_result = decoder.decode(v_bytes.decode_bytes())
        if decode_result._1 == 0xFFFD then
          decode_error = true
        end
        byte_consumer.apply(decode_result._1)
        v_bytes.process_bytes(decode_result._2)
      end
    end

    while v_bytes.bytes_loaded() > 0 do
      let decode_result = decoder.decode(v_bytes.decode_bytes())
      if decode_result._1 == 0xFFFD then
        decode_error = true
      end
      byte_consumer.apply(decode_result._1)
      v_bytes.process_bytes(decode_result._2)
    end

    decode_error

class StringRunes is Iterator[U32]
  let _string: String box
  var _i: USize

  new create(string: String box) =>
    _string = string
    _i = 0

  fun has_next(): Bool =>
    _i < _string.byte_size()

  fun ref next(): U32 ? =>
    (let rune, let len) = _string._codepoint(_i)?
    _i = _i + len.usize()
    rune

class StringBytes is Iterator[U8]
  let _string: String box
  let _encoder: StringEncoder val
  var _i: USize = 0
  var _byte_pos: USize = 0

  new create(string: String box, encoder: StringEncoder) =>
    _string = string
    _encoder = encoder

  fun has_next(): Bool =>
    _i < _string.byte_size()

  fun ref next(): U8 ? =>
    if  _encoder is UTF8StringEncoder then
      if _i < _string.byte_size() then
        let b = _string._byte(_i)
        _i = _i + 1
        return b
      else
        error
      end
    else
      (let cp, let sz) = _string._codepoint(_i)?
      (let byte_size, let byte_u32) = _encoder.encode(cp)
      if _byte_pos == byte_size then
        _i = _i + sz.usize()
        _byte_pos = 0
        return next()?
      else
        let result = ((byte_u32 >> (_byte_pos * 8).u32()) and 0xFF).u8()
        _byte_pos = _byte_pos + 1
        return result
      end
    end

class _LimittedIterator[A] is Iterator[A]
  let _iter: Iterator[A]
  var _limit: USize

  new create(iter: Iterator[A], limit: USize) =>
    _iter = iter
    _limit = limit

  fun ref has_next(): Bool =>
    _iter.has_next() and (_limit > 0)

  fun ref next(): A ? =>
    if has_next() then
      _limit = _limit - 1
      return _iter.next()?
    end
    error
