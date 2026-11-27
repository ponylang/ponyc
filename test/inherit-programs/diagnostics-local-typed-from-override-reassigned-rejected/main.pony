// C's mk() returns P, so in C's copy of f() the local v is a P, and C's mk3()
// returns None, which can't be assigned to v. Expects "right side must be a
// subtype of left side" with a note naming method 'f' inherited by C from Foo.
primitive P
trait Foo
  fun mk(): (P | None)
  fun mk3(): (P | None)
  fun f(): String =>
    var v = mk()
    v = mk3()
    if v is None then "none" else "p" end
class C is Foo
  new create() => None
  fun mk(): P => P
  fun mk3(): None => None
actor Main
  new create(env: Env) => env.out.print(C.f())
