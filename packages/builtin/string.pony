interface Stringable box
  """
  Things that can be turned into a String.
  """
  fun box string(): String

class String val is Ordered[String], Hashable, Stringable
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

  new from_cstring(str: Pointer[U8] ref) =>
    """
    The cstring is not copied, so this should be done with care.
    """
    _size = 0

    while str._apply(_size) != 0 do
      _size = _size + 1
    end

    _alloc = _size + 1
    _ptr = str

  new copy_cstring(str: Pointer[U8] ref) =>
    """
    The cstring is copied, so this is always safe.
    """
    _size = 0

    while str._apply(_size) != 0 do
      _size = _size + 1
    end

    _alloc = _size + 1
    _ptr = Pointer[U8]._create(_alloc)
    _ptr._copy(0, str, _alloc)

  new from_i8(x: I8, base: U8) =>
    _size = 0
    _alloc = 0
    _ptr = Pointer[U8].null()
    from_i64_in_place(x.i64(), base)

  new from_i16(x: I16, base: U8) =>
    _size = 0
    _alloc = 0
    _ptr = Pointer[U8].null()
    from_i64_in_place(x.i64(), base)

  new from_i32(x: I32, base: U8) =>
    _size = 0
    _alloc = 0
    _ptr = Pointer[U8].null()
    from_i64_in_place(x.i64(), base)

  new from_i64(x: I64, base: U8) =>
    _size = 0
    _alloc = 0
    _ptr = Pointer[U8].null()
    from_i64_in_place(x, base)

  new from_i128(x: I128, base: U8) =>
    _size = 0
    _alloc = 0
    _ptr = Pointer[U8].null()
    from_i128_in_place(x, base)

  new from_u8(x: U8, base: U8) =>
    _size = 0
    _alloc = 0
    _ptr = Pointer[U8].null()
    from_u64_in_place(x.u64(), base)

  new from_u16(x: U16, base: U8) =>
    _size = 0
    _alloc = 0
    _ptr = Pointer[U8].null()
    from_u64_in_place(x.u64(), base)

  new from_u32(x: U32, base: U8) =>
    _size = 0
    _alloc = 0
    _ptr = Pointer[U8].null()
    from_u64_in_place(x.u64(), base)

  new from_u64(x: U64, base: U8) =>
    _size = 0
    _alloc = 0
    _ptr = Pointer[U8].null()
    from_u64_in_place(x, base)

  new from_u128(x: U128, base: U8) =>
    _size = 0
    _alloc = 0
    _ptr = Pointer[U8].null()
    from_u128_in_place(x, base)

  new from_f32(x: F32) =>
    _size = 0
    _alloc = 0
    _ptr = Pointer[U8].null()
    from_f64_in_place(x.f64())

  new from_f64(x: F64) =>
    _size = 0
    _alloc = 0
    _ptr = Pointer[U8].null()
    from_f64_in_place(x)

  new _empty() =>
    _size = 0
    _alloc = 0
    _ptr = Pointer[U8].null()

  new prealloc(size: U64) =>
    _size = size
    _alloc = size + 1
    _ptr = Pointer[U8]._create(_alloc)
    _ptr._update(size, 0)

  fun box cstring(): Pointer[U8] tag => _ptr

  fun box length(): U64 => _size

  fun ref reserve(len: U64): String ref^ =>
    if _alloc < len then
      _alloc = len.max(8).next_pow2()
      _ptr = _ptr._realloc(_alloc)
    end
    this

  fun ref recalc(): String ref^ =>
    _size = 0

    while (_size < _alloc) and (_ptr._apply(_size) > 0) do
      _size = _size + 1
    end
    this

  fun box apply(i: I64): U8 ? =>
    let j = offset_to_index(i)
    if j < _size then _ptr._apply(j) else error end

  fun ref update(i: I64, value: U8): U8 ? =>
    let j = offset_to_index(i)

    if j < _size then
      if value == 0 then
        _size = j
      end

      _ptr._update(j, value)
    else
      error
    end

  fun ref _set(i: U64, char: U8): U8 =>
    """
    Unsafe update, used internally.
    """
    _ptr._update(i, char)

  fun box clone(): String iso^ =>
    let len = _size
    let str = recover String.prealloc(len) end
    var i: U64 = 0

    while i < len do
      let c = _ptr._apply(i)
      str._set(i, c)
      i = i + 1
    end

    consume str

  fun box find(s: String box, offset: I64 = 0): I64 ? =>
    """
    Return the index of the first instance of s in the string. Raise an error
    if there are no occurences of s or s is empty.
    """
    var i = offset_to_index(offset)

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

      if same then
        return i.i64()
      end

      i = i + 1
    end
    error

  fun box rfind(s: String, offset: I64 = -1): I64 ? =>
    """
    Return the index of the last instance of s in the string. Raise an error
    if there are no occurences of s or s is empty.
    """
    var i = offset_to_index(offset) - s._size

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

      if same then
        return i.i64()
      end

      i = i - 1
    end
    error

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
    var i: U64 = 0
    var j = _size - 1

    while i < j do
      let x = _ptr._apply(i)
      _ptr._update(i, _ptr._apply(j))
      _ptr._update(j, x)
      i = i + 1
      j = j - 1
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

    if(start < _size) and (start <= finish) and (finish < _size) then
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
    let ptr = _ptr._concat(_size, that._ptr, that._size + 1)

    recover
      let str = String._empty()
      str._size = len
      str._alloc = len + 1
      str._ptr = consume ptr
      consume str
    end

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
    if((_size + that._size) >= _alloc) then
      _alloc = _size + that._size + 1
      _ptr = _ptr._realloc(_alloc)
    end
    _ptr._copy(_size, that._ptr, that._size + 1)
    _size = _size + that._size
    this

  fun ref append_byte(value: U8): String ref^ =>
    if((_size + 1) >= _alloc) then
      _alloc = _size + 2
      _ptr = _ptr._realloc(_alloc)
    end
    _ptr._update(_size, value)
    _ptr._update(_size + 1, 0)
    _size = _size + 1
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

  fun tag _int_table(): String =>
    "zyxwvutsrqponmlkjihgfedcba9876543210123456789abcdefghijklmnopqrstuvwxyz"

  fun ref from_i64_in_place(x: I64, base: U8): String ref^ =>
    if (base < 2) or (base > 36) then
      _size = 0
      _alloc = 1
      _ptr = _ptr._realloc(1)
      _ptr._update(0, 0)
    else
      _size = 0
      _alloc = 32
      _ptr = _ptr._realloc(32)

      let table = _int_table()
      var value = x
      let div = base.i64()
      var i: U64 = 0

      repeat
        let tmp = value
        value = value / div
        let index = (tmp - (value * div)).u64()
        _ptr._update(i, table._ptr._apply(index + 35))
        i = i + 1
      until value == 0 end

      if x < 0 then
        _ptr._update(i, 45)
        i = i + 1
      end

      _ptr._update(i, 0)
      _size = i
      reverse_in_place()
    end
    this

  fun ref from_u64_in_place(x: U64, base: U8): String ref^ =>
    if (base < 2) or (base > 36) then
      _size = 0
      _alloc = 1
      _ptr = _ptr._realloc(1)
      _ptr._update(0, 0)
    else
      _size = 0
      _alloc = 32
      _ptr = _ptr._realloc(32)

      let table = _int_table()
      var value = x
      let div = base.u64()
      var i: U64 = 0

      repeat
        let tmp = value
        value = value / div
        let index = tmp - (value * div)
        _ptr._update(i, table._ptr._apply(index + 35))
        i = i + 1
      until value == 0 end

      _ptr._update(i, 0)
      _size = i
      reverse_in_place()
    end
    this

  fun ref from_i128_in_place(x: I128, base: U8): String ref^ =>
    if (base < 2) or (base > 36) then
      _size = 0
      _alloc = 1
      _ptr = _ptr._realloc(1)
      _ptr._update(0, 0)
    else
      _size = 0
      _alloc = 64
      _ptr = _ptr._realloc(64)

      let table = _int_table()
      var value = x
      let div = base.i128()
      var i: U64 = 0

      repeat
        let tmp = value
        value = value / div
        let index = (tmp - (value * div)).u64()
        _ptr._update(i, table._ptr._apply(index + 35))
        i = i + 1
      until value == 0 end

      if x < 0 then
        _ptr._update(i, 45)
        i = i + 1
      end

      _ptr._update(i, 0)
      _size = i
      reverse_in_place()
    end
    this

  fun ref from_u128_in_place(x: U128, base: U8): String ref^ =>
    if (base < 2) or (base > 36) then
      _size = 0
      _alloc = 1
      _ptr = _ptr._realloc(1)
      _ptr._update(0, 0)
    else
      _size = 0
      _alloc = 64
      _ptr = _ptr._realloc(64)

      let table = _int_table()
      var value = x
      let div = base.u128()
      var i: U64 = 0

      repeat
        let tmp = value
        value = value / div
        let index = (tmp - (value * div)).u64()
        _ptr._update(i, table._ptr._apply(index + 35))
        i = i + 1
      until value == 0 end

      _ptr._update(i, 0)
      _size = i
      reverse_in_place()
    end
    this

  fun ref from_f64_in_place(x: F64): String ref^ =>
    _alloc = 32
    _ptr = _ptr._realloc(32)
    if Platform.windows() then
      _size = @_snprintf[I32](_ptr, _alloc, "%.14g".cstring(), x).u64()
    else
      _size = @snprintf[I32](_ptr, _alloc, "%.14g".cstring(), x).u64()
    end
    this

  fun box hash(): U64 => @hash_block[U64](_ptr, _size)

  fun box string(): String =>
    let len = _size
    let ptr = _ptr._concat(_size + 1, Pointer[U8].null(), 0)

    recover
      let str = String._empty()
      str._size = len
      str._alloc = len + 1
      str._ptr = consume ptr
      consume str
    end
