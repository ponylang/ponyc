trait Iterator[A]
  fun has_next{iso|var|val}():Bool
  fun next{var}():A->this

trait Iterable[A]
  fun iterator{var|val}():Iterator[A]
