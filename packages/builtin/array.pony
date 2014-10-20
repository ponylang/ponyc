class Array[A]
  var _size: U64
  var _alloc: U64
  var _ptr: Pointer[A]

  new create() =>
    _size = 0
    _alloc = 0
    _ptr = Pointer[A]._null()

  new init(with: A, len: U64) =>
    _size = len
    _alloc = len
    _ptr = Pointer[A]._create(len)

    for i in Range[U64](0, len) do
      _ptr._update(i, with)
    end

  new from_carray(ptr: Pointer[A] ref, len: U64) =>
    _size = len
    _alloc = len
    _ptr = ptr

  // Replace this with a default argument on create().
  new prealloc(len: U64) =>
    _size = 0
    _alloc = len
    _ptr = Pointer[A]._create(len)

  fun box length(): U64 => _size

  fun box carray(): Pointer[A] box => _ptr

  fun box apply(i: U64): this->A ? =>
    if i < _size then
      _ptr._apply(i)
    else
      error
    end

  fun ref update(i: U64, v: A): A^ ? =>
    if i < _size then
      _ptr._update(i, v)
    else
      error
    end

  fun ref reserve(len: U64): Array[A] =>
    if _alloc < len then
      _alloc = len.max(8).next_pow2()
      _ptr = _ptr._realloc(_alloc)
    end
    this

  fun ref clear() => _size = 0

  fun ref append(v: A): Array[A] =>
    reserve(_size + 1)
    _ptr._update(_size, v)
    _size = _size + 1
    this

  fun ref concat(iter: Iterator[A] ref) =>
    try for v in iter do append(v) end end

  fun box keys(): ArrayKeys[A, this->Array[A]]^ =>
    ArrayKeys[A, this->Array[A]](this)

  fun box values(): ArrayValues[A, this->Array[A]]^ =>
    ArrayValues[A, this->Array[A]](this)

  fun box pairs(): ArrayPairs[A, this->Array[A]]^ =>
    ArrayPairs[A, this->Array[A]](this)

class ArrayKeys[A, B: Array[A] box] is Iterator[U64]
  var _array: B
  var _i: U64

  new create(array: B) =>
    _array = array
    _i = 0

  fun box has_next(): Bool => _i < _array.length()

  fun ref next(): U64 =>
    if _i < _array.length() then
      _i = _i + 1
    else
      _i
    end

class ArrayValues[A, B: Array[A] box] is Iterator[B->A]
  var _array: B
  var _i: U64

  new create(array: B) =>
    _array = array
    _i = 0

  fun box has_next(): Bool => _i < _array.length()

  fun ref next(): B->A ? => _array(_i = _i + 1)

class ArrayPairs[A, B: Array[A] box] is Iterator[(U64, B->A)]
  var _array: B
  var _i: U64

  new create(array: B) =>
    _array = array
    _i = 0

  fun box has_next(): Bool => _i < _array.length()

  fun ref next(): (U64, B->A) ? => (_i, _array(_i = _i + 1))
