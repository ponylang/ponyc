class POINTER[A]

class Array[A] is Iterable[A]
  var size: U64
  var alloc: U64
  var ptr: POINTER[A]

  new create() ->
    size := 0
    alloc := 0
    ptr := POINTER[A](0)

  fun:box length(): U64 -> size

  fun:box? apply(i: U64): this.A ->
    if i < size then ptr(i) else undef end

  fun:var update(i: U64, v: A): this.A^ ->
    if i < size then ptr(i) := v else v end

  fun:var reserve(len: U64): Array[A]:this ->
    if alloc < len then
      alloc := len.max(8).next_pow2()
      ptr := POINTER[A].from(ptr, alloc)
    end
    this

  fun:var clear(): Array[A]:this ->
    size := 0
    this

  fun:var append(v: A): Array[A]:this ->
    reserve(size + 1)
    ptr(size) := v
    size := size + 1
    this

  fun:var concat(iter: Iterable[A]:var): Array[A]:this ->
    for v in iter do append(v) end
    this

  fun:box iterator(): ArrayIterator[A, Array[A]:this]:var^ ->
    ArrayIterator(this)

class ArrayIterator[A, B:Array[A]:box] is Iterator[A]
  var array: B
  var i: U64

  new create(array': B) ->
    array := array'
    i := 0

  fun:box has_next(): Bool -> i < array.length()

  fun:var? next(): array.A -> array(i := i + 1)
