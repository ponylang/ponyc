trait Iterator[A, p]
{
  read has_next() Bool
  write next() A->p
}

trait Iterable[A]
{
  read iterator() Iterator[A, this]<write>
}
