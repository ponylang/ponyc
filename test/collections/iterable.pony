trait Iterator[A]
  fun box has_next(): Bool
  fun ref next(): A ?

trait Iterable[A]
  fun box iterator(): Iterator[this->A] ref^
