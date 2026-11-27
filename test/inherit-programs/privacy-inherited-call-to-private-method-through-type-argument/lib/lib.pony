trait HasSecret
  fun _secret(): String

trait Foo[A: HasSecret val]
  fun f(a: A): String => a._secret()
