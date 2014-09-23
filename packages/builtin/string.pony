trait Stringable
  fun box string(): String

class String val is Ordered[String]
  var _size: U64
  var _alloc: U64
  var _ptr: Pointer[U8]

  new create() =>
    _size = 0
    _alloc = 1
    _ptr = Pointer[U8](1)
    _ptr._update(0, 0)

  new concat(a: String, b: String) =>
    _size = a._size + b._size
    _alloc = _size + 1
    _ptr = Pointer[U8](_alloc)
    _ptr._copy(0, a._ptr, a._size)
    _ptr._copy(a._size, b._ptr, b._size + 1)

  new from_cstring(str: Pointer[U8] box) =>
    _size = 0

    while str._apply(_size) != 0 do
      _size = _size + 1
    end

    _alloc = _size + 1
    _ptr = Pointer[U8](_alloc)
    _ptr._copy(0, str, _alloc)

  fun box length(): U64 => _size

  fun box cstring(): this->Pointer[U8] => _ptr

  fun box apply(i: U64): U8 ? =>
    if i < _size then _ptr._apply(i) else error end

  fun ref update(i: U64, char: U8): U8 ? =>
    if i < _size then _ptr._update(i, char) else error end

  fun box lower(): String iso^ =>
    recover
      var str = String
      str._size = _size
      str._alloc = _size + 1
      str._ptr = str._ptr._realloc(_size + 1)

      for i in Range[U64](0, _size) do
        var c = _ptr._apply(i)

        if (c >= 0x41) and (c <= 0x5A) then
          c = c + 0x20
        end

        str._ptr._update(i, c)
      end

      consume str
    end

  fun box upper(): String iso^ =>
    recover
      var str = String
      str._size = _size
      str._alloc = _size + 1
      str._ptr = str._ptr._realloc(_size + 1)

      for i in Range[U64](0, _size) do
        var c = _ptr._apply(i)

        if (c >= 0x61) and (c <= 0x7A) then
          c = c - 0x20
        end

        str._ptr._update(i, c)
      end

      consume str
    end

  fun box reverse(): String iso^ =>
    recover
      var str = String
      str._size = _size
      str._alloc = _size + 1
      str._ptr = str._ptr._realloc(_size + 1)

      for i in Range[U64](0, _size) do
        var c = _ptr._apply(i)
        str._ptr._update(_size - i - 1, c)
      end

      consume str
    end

  fun box sub(from: I64, to: I64): String iso^ =>
    recover
      var start =
        if from < 0 then from.u64() + _size else from.u64() end

      var finish =
        (if to < 0 then to.u64() + _size + 1 else to.u64() end).min(_size - 1)

      var str = String

      if (start < _size) and (start <= finish) then
        var len = (finish - start) + 1
        str._size = len
        str._alloc = len + 1
        str._ptr = str._ptr._realloc(len + 1)

        for i in Range[U64](start, finish + 1) do
          var c = _ptr._apply(i)
          str._ptr._update(i - start, c)
        end
      end

      consume str
    end

  fun box eq(that: String box): Bool =>
    if _size == that._size then
      for i in Range[U64](0, _size) do
        if _ptr._apply(i) != that._ptr._apply(i) then
          return False
        end
      end
      True
    else
      False
    end

  fun box lt(that: String box): Bool =>
    var len = _size.min(that._size)
    for i in Range[U64](0, len) do
      if _ptr._apply(i) < that._ptr._apply(i) then
        return True
      elseif _ptr._apply(i) > that._ptr._apply(i) then
        return False
      end
    end
    _size < that._size

  fun box le(that: String box): Bool =>
    var len = _size.min(that._size)
    for i in Range[U64](0, len) do
      if _ptr._apply(i) < that._ptr._apply(i) then
        return True
      elseif _ptr._apply(i) > that._ptr._apply(i) then
        return False
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

  /*fun box string(): String => this*/
