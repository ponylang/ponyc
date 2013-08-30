trait Iterator[A]
  fun has_next():Bool
  fun next():A->this

trait Iterable[A]
  fun iterator{var|val}():Iterator[A]
