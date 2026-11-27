// C's mk() returns U8, so in C's copy of f() the local v is a U8 and v =
// None is wrong. Expects "right side must be a subtype of left side" with a
// note naming method 'f' inherited by C from Foo.
trait Foo
  fun mk(): (U8 | None)
  fun f(): String =>
    var v = mk()
    v = None
    "ok"
class C is Foo
  new create() => None
  fun mk(): U8 => 3
actor Main
  new create(env: Env) => env.out.print(C.f())
