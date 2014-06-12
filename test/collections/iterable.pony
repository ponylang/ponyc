trait Iterator[Z]
  fun box has_next(): Bool
  fun ref next(): Z this ?

trait Iterable[Z]
  fun box iterator(): Iterator[Z] ref^
