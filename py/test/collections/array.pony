class Array[A] is Iterable[A]
{
  var size:U64
  var alloc:U64
  var POINTER[A]

  new default() = size = 0; alloc = 0; POINTER = 0; this

  read size() U64 = size

  read apply(i:U64) A->this throw = if i < size then POINTER(i) else throw

  write update(i:U64, v:A) A throw =
    if i < size then swap(POINTER(i), v) else throw

  write clear() None = size = 0; None

  write append(v:A) None =
    if size == alloc then {
      if alloc == 0 then alloc = 4 else alloc = alloc * 2;
      POINTER.resize(alloc)
    } else {
      POINTER
    }(size) = v;
    size = size + 1;
    None

  write append(iter:Iterable[A]) None = for v in iter do append(v); None

  read iterator() ArrayIterator[A, this]<wri> = ArrayIterator(this)
}

class ArrayIterator[A, p:wri|fro] is Iterator[A, p]
{
  var array:Array[A]<p>
  var i:U64

  new default(array:Array[A]<p>) = i = 0; this

  read has_next() Bool = i < array.size()

  write next() A->p =
    var item = array(i);
    i = i + 1;
    item
}
