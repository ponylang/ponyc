trait Iterator[A]
  fun has_next():Bool
  fun next():A->this

trait Iterable[A]
  fun{var|val} iterator():Iterator[A]
