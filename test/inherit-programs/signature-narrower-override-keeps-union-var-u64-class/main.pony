// T's default stores get() in a (U64 | None) var and then assigns None. C
// overrides get() to return U64. The test expects "ok": the var keeps its
// declared union type in the inherited body.
trait T
  fun get(): (U64 | None)
  fun f(): String =>
    var x: (U64 | None) = get()
    x = None
    "ok"
class C is T
  new create() => None
  fun get(): U64 => 1
actor Main
  new create(env: Env) => env.out.print(C.f())
