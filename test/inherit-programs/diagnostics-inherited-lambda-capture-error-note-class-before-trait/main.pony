// Foo.f() captures me = this in a lambda. In C's copy me is a C box, which is
// not a subtype of the C ref^ the capture needs. C is declared before Foo.
// The program must fail with "right side must be a subtype of left side" and
// a note naming f() inherited by C from Foo.
class C is Foo
  new create() => None
  fun g(): U8 => 7
trait Foo
  fun g(): U8 ?
  fun f(): U8 =>
    let me = this
    let h = {()(me): U8 ? => me.g()?}
    try h()? else 0 end
actor Main
  new create(env: Env) => env.out.print(C.f().string())
