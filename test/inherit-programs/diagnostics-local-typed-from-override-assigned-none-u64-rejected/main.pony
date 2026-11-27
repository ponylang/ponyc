// C's get() returns U64, so in C's copy of f() the local x is a U64 and x =
// None is wrong. Expects "right side must be a subtype of left side" with a
// note naming method 'f' inherited by C from T.
trait T
  fun get(): (U64 | None)
  fun f(): String =>
    var x = get()
    x = None
    "ok"
class C is T
  new create() => None
  fun get(): U64 => 1
actor Main
  new create(env: Env) => env.out.print(C.f())
