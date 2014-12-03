class Array[A]
  var _size: U64
  var _alloc: U64
  var _ptr: Pointer[A]

  new create(len: U64 = 0) =>
    _size = 0
    _alloc = len
    _ptr = Pointer[A]._create(len)

  // Put this in once we have codegen for polymorphic methods.
  // new init[B: (A & Any box)](from: B, len: U64) =>
  //  _size = len
  //  _alloc = len
  //  _ptr = Pointer[A]._create(len)
  //
  //  var i: U64 = 0
  //
  //  while i < len do
  //    _ptr._update(i, from)
  //    i = i + 1
  //  end

  // Put this in once we have codegen for polymorphic methods.
  new undefined/*[B: (A & Real[B] & Number)]*/(len: U64) =>
    _size = len
    _alloc = len
    _ptr = Pointer[A]._create(len)

  new from_cstring(ptr: Pointer[A] ref, len: U64) =>
    _size = len
    _alloc = len
    _ptr = ptr

  fun box cstring(): Pointer[A] tag => _ptr

  fun box size(): U64 => _size

  fun box space(): U64 => _alloc

  fun ref reserve(len: U64): Array[A]^ =>
    if _alloc < len then
      _alloc = len.max(8).next_pow2()
      _ptr = _ptr._realloc(_alloc)
    end
    this

  // Use only to test performance without bounds checking.
  // This is in preparation for bounds checking elimination in the compiler.
  fun box unsafe_get(i: U64): this->A =>
    _ptr._apply(i)

  fun ref unsafe_set(i: U64, value: A): A^ =>
    _ptr._update(i, consume value)

  fun box apply(i: U64): this->A ? =>
    if i < _size then
      _ptr._apply(i)
    else
      error
    end

  fun ref update(i: U64, value: A): A^ ? =>
    if i < _size then
      _ptr._update(i, consume value)
    else
      error
    end

  fun ref delete(i: U64): A^ ? =>
    if i < _size then
      _size = _size - 1
      _ptr._delete(i, 1, _size - i)
    else
      error
    end

  fun ref truncate(len: U64): Array[A]^ =>
    _size = _size.min(len)
    this

  fun ref clear(): Array[A]^ =>
    _size = 0
    this

  fun ref append(value: A): Array[A]^ =>
    reserve(_size + 1)
    _ptr._update(_size, consume value)
    _size = _size + 1
    this

  //Put this in once we have codegen for polymorphic methods.
  //fun ref concat[B: (A & Any box)](iter: Iterator[B]) =>
  //  try
  //    for v in iter do
  //      append(v)
  //    end
  //  end

  fun box keys(): ArrayKeys[A, this->Array[A]]^ =>
    ArrayKeys[A, this->Array[A]](this)

  fun box values(): ArrayValues[A, this->Array[A]]^ =>
    ArrayValues[A, this->Array[A]](this)

  fun box pairs(): ArrayPairs[A, this->Array[A]]^ =>
    ArrayPairs[A, this->Array[A]](this)

class ArrayKeys[A, B: Array[A] box] is Iterator[U64]
  let _array: B
  var _i: U64

  new create(array: B) =>
    _array = array
    _i = 0

  fun box has_next(): Bool => _i < _array.size()

  fun ref next(): U64 =>
    if _i < _array.size() then
      _i = _i + 1
    else
      _i
    end

class ArrayValues[A, B: Array[A] box] is Iterator[B->A]
  let _array: B
  var _i: U64

  new create(array: B) =>
    _array = array
    _i = 0

  fun box has_next(): Bool => _i < _array.size()

  fun ref next(): B->A ? => _array(_i = _i + 1)

class ArrayPairs[A, B: Array[A] box] is Iterator[(U64, B->A)]
  let _array: B
  var _i: U64

  new create(array: B) =>
    _array = array
    _i = 0

  fun box has_next(): Bool => _i < _array.size()

  fun ref next(): (U64, B->A) ? => (_i, _array(_i = _i + 1))
