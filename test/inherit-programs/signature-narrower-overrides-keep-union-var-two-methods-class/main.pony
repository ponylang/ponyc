// Foo's default stores mk() in a (P | None) var, assigns mk3() to it, and
// checks for None. C overrides mk() to return P and mk3() to return None. The
// test expects "none": the var keeps its declared union type.
primitive P
trait Foo
  fun mk(): (P | None)
  fun mk3(): (P | None)
  fun f(): String =>
    var v: (P | None) = mk()
    v = mk3()
    if v is None then "none" else "p" end
class C is Foo
  new create() => None
  fun mk(): P => P
  fun mk3(): None => None
actor Main
  new create(env: Env) => env.out.print(C.f())
