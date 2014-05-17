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

  fun box apply(i: U64): this.A ? =>
    if i < size then ptr(i) else error end

  fun mut update(i: U64, v: A): this.A^ =>
    if i < size then ptr(i) = v else v end

  fun mut reserve(len: U64): Array[A] mut^ =>
    if alloc < len then
      alloc = len.max(8).next_pow2()
      ptr = POINTER[A].from(ptr, alloc)
    end
    this

  fun mut clear(): Array[A] mut^ =>
    size = 0
    this

  fun mut append(v: A): Array[A] mut^ =>
    reserve(size + 1)
    ptr(size) = v
    size = size + 1
    this

  fun mut concat(iter: Iterable[A] mut): Array[A] mut^ =>
    for v in iter do append(v) end
    this

  fun box iterator(): ArrayIterator[A, Array[A] this] mut^ =>
    ArrayIterator(this)

class ArrayIterator[A, B:Array[A] box] is Iterator[A]
  var array: B
  var i: U64

  new create(array': B) =>
    array = array'
    i = 0

  fun box has_next(): Bool => i < array.length()

  fun mut next(): array.A ? => array(i = i + 1)
