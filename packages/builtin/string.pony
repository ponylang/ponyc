trait Stringable
  fun box string(): String

class String val is Ordered[String]
  var _size: U64
  var _alloc: U64
  var _ptr: Pointer[U8]

  new from_cstring(str: Pointer[U8] box) =>
    _size = 0

    while str._apply(_size) != 0 do
      _size = _size + 1
    end

    _alloc = _size + 1
    _ptr = Pointer[U8](_alloc)
    _ptr._copy(str, _alloc)

  fun box length(): U64 => _size

  fun box cstring(): this->Pointer[U8] => _ptr

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

  /*fun box string(): String => this*/
