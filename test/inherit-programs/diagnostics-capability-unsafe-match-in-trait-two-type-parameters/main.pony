// T[A val, B ref].f matches (A | B) against A, which could need a runtime
// capability check. The error must be reported once at the trait's code,
// with no note naming C, even though C instantiates both with Foo.
class Foo
  var v: U8 = 0
  new create() => None
trait T[A: Any val, B: Any ref]
  fun f(x: (A | B)): String =>
    match x
    | let y: A => "a"
    else "other" end
class C is T[Foo val, Foo ref]
  new create() => None
actor Main
  new create(env: Env) => env.out.print(C.f(Foo))
