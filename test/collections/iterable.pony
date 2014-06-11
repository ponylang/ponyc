trait Iterator[Z]
  fun box has_next(): Bool
  fun mut next(): this.Z ?

trait Iterable[Z]
  fun box iterator(): Iterator[Z] mut^
