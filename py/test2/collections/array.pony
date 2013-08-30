class Array[A] is Iterable[A]
  var size:U64
  var alloc:U64
  var ptr:POINTER[A]

  fun default() =
    size = 0;
    alloc = 0;
    ptr = POINTER[A](0);
    this

  fun{iso|var|val} length():U64 = size

  fun{var|val} apply(i:U64):A->this throw =
    if i < size then ptr(i) else throw

  fun{var} update(i:U64, v:A):A =
    if i < size then ptr(i) = v else v

  fun{iso|var} reserve(len:U64) =
    if alloc < len then
      (alloc = len.max(8).next_pow2();
      ptr = POINTER[A].from(ptr, alloc))
    end

  fun{iso|var} clear() = size = 0

  fun{var} append(v:A) =
    reserve(size + 1);
    ptr(size) = v;
    size = size + 1

  fun{var} concat(iter:Iterable[A]) = for v in iter do append(v)

  fun{var|val} iterator():ArrayIterator[A, Array[A]{this}] =
    ArrayIterator(this)

class ArrayIterator[A, B:Array[A]]{var} is Iterator[A]
  var array:B
  var i:U64

  fun default(array':B) =
    Partial[This](array', i).absorb()

  fun has_next():Bool = i < array.size()

  fun next():A->array = array(i = i + 1);
