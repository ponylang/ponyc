// T.f's default is a literal that provides T. C overrides name, but the
// literal inherits T's name, so C.f() prints T.
trait T
  fun name(): String => "T"
  fun f(x: T val = object val is T end): String => x.name()
class C is T
  fun name(): String => "C"
actor Main
  new create(env: Env) => env.out.print(C.f())
