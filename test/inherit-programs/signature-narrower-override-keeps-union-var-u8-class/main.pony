// Foo's default stores mk() in a (U8 | None) var and then assigns None. C
// overrides mk() to return U8. The test expects "ok": the var keeps its
// declared union type in the inherited body.
trait Foo
  fun mk(): (U8 | None)
  fun f(): String =>
    var v: (U8 | None) = mk()
    v = None
    "ok"
class C is Foo
  new create() => None
  fun mk(): U8 => 3
actor Main
  new create(env: Env) => env.out.print(C.f())
