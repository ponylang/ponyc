trait Iterator[A]
  fun:box has_next(): Bool
  fun:var? next(): this.A

trait Iterable[A]
  fun:box iterator(): Iterator[A]:var^
