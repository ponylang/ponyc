trait Iterator[A, p]<wri>
{
  read has_next()->(Bool)
  write next()->(A p)
}

trait Iterable[A]
{
  read iterator()->(Iterator[A, this])
}
