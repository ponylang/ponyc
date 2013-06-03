class Array[A] is Iterable[A]
{
  var oob:A
  var size:U64
  var alloc:U64
  var ptr:POINTER[A]

  new default(oob:A = A()) =
    size = 0;
    alloc = 0;
    ptr = POINTER[A](0);
    this

  read size()->(U64) = size

  read apply(i:U64)->(A this) = if i < size then ptr(i) else oob

  write update(i:U64, v:A)->(Bool) =
    if i < size then {ptr(i) = v; true} else false

  write reserve(len:U64) =
    if alloc < len then {
      alloc = len.max(8).next_pow2();
      ptr = POINTER[A].from(ptr, alloc)
    } else None

  write clear() = size = 0

  write append(v:A) =
    reserve(size + 1);
    ptr(size) = v;
    size = size + 1

  write append(iter:Iterable[A]) = for v in iter do append(v)

  read iterator()->(ArrayIterator[A, this]) = ArrayIterator(this)
}

class ArrayIterator[A, p:wri|fro] is Iterator[A, p]
{
  var array:Array[A]<p>
  var i:U64

  new default(array:Array[A]<p>) = i = 0; this

  read has_next()->(Bool) = i < array.size()

  write next()->(A p) =
    var item = array(i);
    i = i + 1;
    item
}
