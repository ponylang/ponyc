trait HasSecret
  fun _secret(): String

trait Foo[A: HasSecret val]
  fun f(a: A): String => a._secret()

primitive G
  fun g[A: HasSecret val](a: A): String => a._secret()
