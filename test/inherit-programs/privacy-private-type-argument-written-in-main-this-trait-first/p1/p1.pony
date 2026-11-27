interface HasName
  fun name(): String

trait T1[A: HasName val]
  fun say(x: A): String => x.name()
