class Pointer[A]
  new create(len: U64) => compiler_intrinsic

  fun ref _realloc(len: U64): Pointer[A] => compiler_intrinsic
  fun box _apply(i: U64): this->A => compiler_intrinsic
  fun ref _update(i: U64, v: A): A^ => compiler_intrinsic
  fun ref _copy(src: Pointer[A] box, len: U64): U64 => compiler_intrinsic

class Array[A]
  var size: U64
  var alloc: U64
  var ptr: Pointer[A]

  new create() =>
    size = 0
    alloc = 0
    ptr = Pointer[A](0)

  fun box length(): U64 => size

  fun box carray(): this->Pointer[A] => ptr

  fun box apply(i: U64): this->A ? =>
    if i < size then
      ptr._apply(i)
    else
      error
    end

  fun ref update(i: U64, v: A): A^ ? =>
    if i < size then
      ptr._update(i, v)
    else
      error
    end

  fun ref reserve(len: U64) =>
    if alloc < len then
      alloc = len.max(8).next_pow2()
      ptr = ptr._realloc(alloc)
    end

  fun ref clear() => size = 0

  fun ref append(v: A): Array[A] =>
    reserve(size + 1)
    ptr._update(size, v)
    size = size + 1
    this

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
