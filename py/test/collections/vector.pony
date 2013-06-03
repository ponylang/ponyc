class Vector[A, d:U64] is Iterable[A]
{
  var POINTER[A]

  new default(?) = this

  read apply(i:Int[0, d]) A->this = POINTER(i)

  write update(i:Int[0, d], v:A) A = swap(POINTER(i), v)

  read iterator() VectorIterator[A, d, this]<wri> = VectorIterator(this)
}

class VectorIterator[A, d:U64, p:wri|fro] is Iterator[A, p]
{
  var vec:Vector[A, d]<p>
  var i:Int[0, d]

  new default(vec:Vector[A, d]<p>) = i = 0; this

  read has_next() Bool = i < d

  write next() A->p =
    var item = vec(i);
    i = i + 1;
    item
}

class Index[max:U64]
{
  var
}
