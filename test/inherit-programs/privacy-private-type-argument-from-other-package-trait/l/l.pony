interface val HasName
  fun name(): String

trait T[A: HasName val]
  fun say(x: A): String => x.name()
