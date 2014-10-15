trait Stringable
  fun box string(): String

class String val is Ordered[String], Hashable[String]
  var _size: U64
  var _alloc: U64
  var _ptr: Pointer[U8]

  new create() =>
    _size = 0
    _alloc = 1
    _ptr = Pointer[U8](1)
    _ptr._update(0, 0)

  new from_cstring(str: Pointer[U8] box) =>
    _size = 0

    while str._apply(_size) != 0 do
      _size = _size + 1
    end

    _alloc = _size + 1
    _ptr = Pointer[U8](_alloc)
    _ptr._copy(0, str, _alloc)

  new from_i8(x: I8, base: U8) =>
    _size = 0
    _alloc = 0
    _ptr = Pointer[U8](0)
    from_i64_in_place(x.i64(), base)

  new from_i16(x: I16, base: U8) =>
    _size = 0
    _alloc = 0
    _ptr = Pointer[U8](0)
    from_i64_in_place(x.i64(), base)

  new from_i32(x: I32, base: U8) =>
    _size = 0
    _alloc = 0
    _ptr = Pointer[U8](0)
    from_i64_in_place(x.i64(), base)

  new from_i64(x: I64, base: U8) =>
    _size = 0
    _alloc = 0
    _ptr = Pointer[U8](0)
    from_i64_in_place(x, base)

  new from_i128(x: I128, base: U8) =>
    _size = 0
    _alloc = 0
    _ptr = Pointer[U8](0)
    from_i128_in_place(x, base)

  new from_u8(x: U8, base: U8) =>
    _size = 0
    _alloc = 0
    _ptr = Pointer[U8](0)
    from_u64_in_place(x.u64(), base)

  new from_u16(x: U16, base: U8) =>
    _size = 0
    _alloc = 0
    _ptr = Pointer[U8](0)
    from_u64_in_place(x.u64(), base)

  new from_u32(x: U32, base: U8) =>
    _size = 0
    _alloc = 0
    _ptr = Pointer[U8](0)
    from_u64_in_place(x.u64(), base)

  new from_u64(x: U64, base: U8) =>
    _size = 0
    _alloc = 0
    _ptr = Pointer[U8](0)
    from_u64_in_place(x, base)

  new from_u128(x: U128, base: U8) =>
    _size = 0
    _alloc = 0
    _ptr = Pointer[U8](0)
    from_u128_in_place(x, base)

  new from_f32(x: F32) =>
    _size = 0
    _alloc = 0
    _ptr = Pointer[U8](0)
    from_f64_in_place(x.f64())

  new from_f64(x: F64) =>
    _size = 0
    _alloc = 0
    _ptr = Pointer[U8](0)
    from_f64_in_place(x)

  new _empty() =>
    _size = 0
    _alloc = 0
    _ptr = Pointer[U8](0)

  new _reserve(size: U64) =>
    _size = size
    _alloc = size + 1
    _ptr = Pointer[U8](_alloc)

  fun box length(): U64 => _size

  fun box cstring(): this->Pointer[U8] => _ptr

  fun box apply(i: I64): U8 ? =>
    var j = offset_to_index(i)
    if j < _size then _ptr._apply(j) else error end

  fun ref update(i: I64, char: U8): U8 ? =>
    var j = offset_to_index(i)

    if j < _size then
      if char == 0 then
        _size = j
      end

      _ptr._update(j, char)
    else
      error
    end

  fun box lower(): String ref^ =>
    var str = String._reserve(_size)

    for i in Range[U64](0, _size) do
      var c = _ptr._apply(i)

      if (c >= 0x41) and (c <= 0x5A) then
        c = c + 0x20
      end

      str._ptr._update(i, c)
    end

    consume str

  fun box upper(): String ref^ =>
    var str = String._reserve(_size)

    for i in Range[U64](0, _size) do
      var c = _ptr._apply(i)

      if (c >= 0x61) and (c <= 0x7A) then
        c = c - 0x20
      end

      str._ptr._update(i, c)
    end

    consume str

  fun box reverse(): String ref^ =>
    var str = String._reserve(_size)

    for i in Range[U64](0, _size) do
      var c = _ptr._apply(i)
      str._ptr._update(_size - i - 1, c)
    end

    consume str

  fun ref reverse_in_place(): String ref =>
    var i: U64 = 0
    var j = _size - 1

    while i < j do
      var x = _ptr._apply(i)
      _ptr._update(i, _ptr._apply(j))
      _ptr._update(j, x)
      i = i + 1
      j = j - 1
    end
    this

  fun box substring(from: I64, to: I64): String ref^ =>
    var start = offset_to_index(from)
    var finish = offset_to_index(to).min(_size)
    var str: String ref

    if (start < _size) and (start < finish) then
      var len = finish - start
      str = String._reserve(len)

      for i in Range[U64](start, finish + 1) do
        var c = _ptr._apply(i)
        str._ptr._update(i - start, c)
      end
    else
      str = String
    end

    consume str

  fun box add(that: String box): String =>
    var len = _size + that._size
    var ptr = _ptr._concat(_size, that._ptr, that._size + 1)

    recover
      var str = String._empty()
      str._size = len
      str._alloc = len + 1
      str._ptr = consume ptr
      consume str
    end

  fun box eq(that: String box): Bool =>
    if _size == that._size then
      for i in Range[U64](0, _size) do
        if _ptr._apply(i) != that._ptr._apply(i) then
          return false
        end
      end
      true
    else
      false
    end

  fun box lt(that: String box): Bool =>
    var len = _size.min(that._size)
    for i in Range[U64](0, len) do
      if _ptr._apply(i) < that._ptr._apply(i) then
        return true
      elseif _ptr._apply(i) > that._ptr._apply(i) then
        return false
      end
    end
    _size < that._size

  fun box le(that: String box): Bool =>
    var len = _size.min(that._size)
    for i in Range[U64](0, len) do
      if _ptr._apply(i) < that._ptr._apply(i) then
        return true
      elseif _ptr._apply(i) > that._ptr._apply(i) then
        return false
      end
    end
    _size <= that._size

  fun ref append(that: String box): String ref =>
    if((_size + that._size) >= _alloc) then
      _alloc = _size + that._size + 1
      _ptr = _ptr._realloc(_alloc)
    end
    _ptr._copy(_size, that._ptr, that._size + 1)
    _size = _size + that._size
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
      @strtoll[I128](_ptr, U64(0),I32(10))
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

  fun ref from_i64_in_place(x: I64, base: U8): String ref =>
    if (base < 2) or (base > 36) then
      _size = 0
      _alloc = 1
      _ptr = _ptr._realloc(1)
      _ptr._update(0, 0)
    else
      _size = 0
      _alloc = 32
      _ptr = _ptr._realloc(32)

      var table = _int_table()
      var value = x
      var div = base.i64()
      var i: U64 = 0

      repeat
        var tmp = value
        value = value / div
        var index = (tmp - (value * div)).u64()
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

  fun ref from_u64_in_place(x: U64, base: U8): String ref =>
    if (base < 2) or (base > 36) then
      _size = 0
      _alloc = 1
      _ptr = _ptr._realloc(1)
      _ptr._update(0, 0)
    else
      _size = 0
      _alloc = 32
      _ptr = _ptr._realloc(32)

      var table = _int_table()
      var value = x
      var div = base.u64()
      var i: U64 = 0

      repeat
        var tmp = value
        value = value / div
        var index = tmp - (value * div)
        _ptr._update(i, table._ptr._apply(index + 35))
        i = i + 1
      until value == 0 end

      _ptr._update(i, 0)
      _size = i
      reverse_in_place()
    end
    this

  fun ref from_i128_in_place(x: I128, base: U8): String ref =>
    if (base < 2) or (base > 36) then
      _size = 0
      _alloc = 1
      _ptr = _ptr._realloc(1)
      _ptr._update(0, 0)
    else
      _size = 0
      _alloc = 64
      _ptr = _ptr._realloc(64)

      var table = _int_table()
      var value = x
      var div = base.i128()
      var i: U64 = 0

      repeat
        var tmp = value
        value = value / div
        var index = (tmp - (value * div)).u64()
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

  fun ref from_u128_in_place(x: U128, base: U8): String ref =>
    if (base < 2) or (base > 36) then
      _size = 0
      _alloc = 1
      _ptr = _ptr._realloc(1)
      _ptr._update(0, 0)
    else
      _size = 0
      _alloc = 64
      _ptr = _ptr._realloc(64)

      var table = _int_table()
      var value = x
      var div = base.u128()
      var i: U64 = 0

      repeat
        var tmp = value
        value = value / div
        var index = (tmp - (value * div)).u64()
        _ptr._update(i, table._ptr._apply(index + 35))
        i = i + 1
      until value == 0 end

      _ptr._update(i, 0)
      _size = i
      reverse_in_place()
    end
    this

  fun ref from_f64_in_place(x: F64): String ref =>
    _alloc = 32
    _ptr = _ptr._realloc(32)
    if Platform.windows() then
      _size = @_snprintf[I32](_ptr, _alloc, "%.14g".cstring(), x).u64()
    else
      _size = @snprintf[I32](_ptr, _alloc, "%.14g".cstring(), x).u64()
    end
    this

  fun box hash(): U64 => @hash_block[U64](_ptr, _size)

  //fun box string(): String =>
