class POINTER[A]

class Array[A] is Iterable[A]
  var size: U64
  var alloc: U64
  var ptr: POINTER[A]

  new create() =>
    size = 0
    alloc = 0
    ptr = POINTER[A](0)

  fun box length(): U64 => size

  fun box apply(i: U64): A this ? =>
    if i < size then ptr(i) else error end

  fun ref update(i: U64, v: A): A this^ =>
    if i < size then ptr(i) = v else v end

  fun ref reserve(len: U64): Array[A] ref^ =>
    if alloc < len then
      alloc = len.max(8).next_pow2()
      ptr = POINTER[A].from(ptr, alloc)
    end
    this

  fun ref clear(): Array[A] ref^ =>
    size = 0
    this

  fun ref append(v: A): Array[A] ref^ =>
    reserve(size + 1)
    ptr(size) = v
    size = size + 1
    this

  fun ref concat(iter: Iterable[A] ref): Array[A] ref^ =>
    for v in iter do append(v) end
    this

  fun box iterator(): ArrayIterator[A, Array[A] this] ref^ =>
    ArrayIterator(this)

class ArrayIterator[A, B: Array[A] box] is Iterator[A]
  var array: B
  var i: U64

  new create(array': B) =>
    array = array'
    i = 0

  fun box has_next(): Bool => i < array.length()

  fun ref next(): A array ? => array(i = i + 1)
