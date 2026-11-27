interface HasName
  fun name(): String
trait T1[A: HasName val]
  fun f(x: A): String => x.name()
