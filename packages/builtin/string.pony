trait Stringable
  fun box string(): String

class String val is Ordered[String]
  var _size: U64
  var _alloc: U64
  var _ptr: _Pointer[U8]

  new _cstring(str: _Pointer[U8] box) =>
    _size = 0

    while str(_size) == 0 do
      _size = _size + 1
    else
      // TODO: Remove this once primitive boxing is implemented.
      0
    end

    _alloc = _size + 1
    _ptr = _Pointer[U8](_alloc)
    _ptr.copy(str, _alloc)

  fun box length(): U64 => _size

  fun box eq(that: String box): Bool =>
    if _size == that._size then
      for i in Range[U64](0, _size) do
        if _ptr(i) != that._ptr(i) then
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
      if _ptr(i) < that._ptr(i) then
        return True
      elseif _ptr(i) > that._ptr(i) then
        return False
      end
    end
    _size < that._size

  fun box le(that: String box): Bool =>
    var len = _size.min(that._size)
    for i in Range[U64](0, len) do
      if _ptr(i) < that._ptr(i) then
        return True
      elseif _ptr(i) > that._ptr(i) then
        return False
      end
    end
    _size <= that._size

  /*fun box string(): String => this*/
