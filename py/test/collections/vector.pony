class Vector[A, d:U64] is Iterable[A]
{
  var ptr:POINTER[A]

  new default(a:A) =
    for i in Indices[d] do ptr(i) = a;
    this

  new from(iter:Iterable[A]) =
    for i in Indices[d] do ptr(i) = iter.next();
    this

  read apply(i:Index[d])->(A this) = ptr(i.value())

  write update(i:Index[d], v:A)->(A) = swap(ptr(i.value()), v)

  read iterator()->(VectorIterator[A, d, this]<wri>) = VectorIterator(this)
}

class VectorIterator[A, d:U64, p:wri|fro] is Iterator[A, p]
{
  var vec:Vector[A, d]<p>
  var i:Index[d]

  new default(vec:Vector[A, d]<p>) = i = Index[d]; this

  read has_next()->(Bool) = i < {d - 1}

  write next()->(A p) = if has_next() then {i = i + 1; vec(i - 1)} else vec(i)
}

class Index[max:U64]<fro>
{
  val i:U64

  /* what if max == 0 */
  new default(i:U64) = i = if i < max then i else {max - 1}; this
  read value()->(U64) = i
  read has_next()->(Bool) = i < {max - 1}
  read next()->(Index[max]) = Index[max](i + 1)
}

class Indices[max:U64]<wri> is Iterable[U64], Iterator[U64, wri]
{
  var i:Index[max]

  new default() = i = Index[max](0); this

  bool has_next()->(Bool) = i.has_next()

  write next()->(Index[max]) = i = i.next()

  write iterator()->(Indices[max]<wri>) = this
}
