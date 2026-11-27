// T.f's default is a literal that provides T, with Main and C declared
// before T. C overrides name, but the literal inherits T's name, so C.f()
// prints T.
actor Main
  new create(env: Env) => env.out.print(C.f())
class C is T
  fun name(): String => "C"
trait T
  fun name(): String => "T"
  fun f(x: T val = object val is T end): String => x.name()
