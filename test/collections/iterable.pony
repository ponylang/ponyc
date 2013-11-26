trait Iterator[A]
  fun{box} has_next():Bool
  fun{var} next():A->this

trait Iterable[A]
  fun{box} iterator():Iterator[A]{var}
