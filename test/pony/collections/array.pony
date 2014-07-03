class _Pointer[A]
  new create(len: U64) => compiler_intrinsic
  new from(ptr: _Pointer[A], len: U64) => compiler_intrinsic
  fun ref apply(i: U64): A => compiler_intrinsic
  fun ref update(i: U64, v: A): A^ => compiler_intrinsic

class Array[A]
  var size: U64
  var alloc: U64
  var ptr: _Pointer[A]

  new create() =>
    size = 0
    alloc = 0
    ptr = _Pointer[A](0)

  fun box length(): U64 => size

  fun box apply(i: U64): this->A ? =>
    if i < size then
      ptr(i)
    else
      error
    end

  fun ref update(i: U64, v: A): this->A^ ? =>
    if i < size then
      ptr(i) = v
    else
      error
    end

  fun ref reserve(len: U64) =>
    if alloc < len then
      alloc = len.max(8).next_pow2()
      ptr = _Pointer[A].from(ptr, alloc)
    end

  fun ref clear() => size = 0

  fun ref append(v: A) =>
    reserve(size + 1)
    ptr(size) = v
    size = size + 1

  fun ref concat(iter: Iterator[A] ref) =>
    for v in iter do append(v) end

  fun box keys(): ArrayKeys[A, this->Array[A]]^ =>
    ArrayKeys[A, this->Array[A]](this)

  fun box values(): ArrayValues[A, this->Array[A]]^ =>
    ArrayValues[A, this->Array[A]](this)

  fun box pairs(): ArrayPairs[A, this->Array[A]]^ =>
    ArrayPairs[A, this->Array[A]](this)

class ArrayKeys[A, B: Array[A] box] is Iterator[U64]
  var array: B
  var i: U64

  new create(array': B) =>
    array = array'
    i = 0

  fun box has_next(): Bool => i < array.length()

  fun ref next(): U64 ? =>
    if i < array.length() then
      i = i + 1
    else
      error
    end

class ArrayValues[A, B: Array[A] box] is Iterator[B->A]
  var array: B
  var i: U64

  new create(array': B) =>
    array = array'
    i = 0

  fun box has_next(): Bool => i < array.length()

  fun ref next(): B->A ? => array(i = i + 1)

class ArrayPairs[A, B: Array[A] box] is Iterator[(U64, B->A)]
  var array: B
  var i: U64

  new create(array': B) =>
    array = array'
    i = 0

  fun box has_next(): Bool => i < array.length()

  fun ref next(): (U64, B->A) ? => (i, array(i = i + 1))
