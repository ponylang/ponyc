trait Iterator[A]
  fun box has_next(): Bool
  fun mut next(): this.A ?

trait Iterable[A]
  fun box iterator(): Iterator[A] mut^
