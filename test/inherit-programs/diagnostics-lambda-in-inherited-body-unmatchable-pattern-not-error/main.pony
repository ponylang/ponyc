// A lambda in Foo's f() matches me.mk() against None, and C's mk() returns
// only U8, so in C's copy that pattern can never match. Expects the program to
// compile and print 3: the dead pattern is in inherited code, not reported.
trait val Foo
  fun mk(): (U8 | None)
  fun val f(): String =>
    let g = {()(me = this): String => match me.mk() | None => "none" | let u: U8 => u.string() end}
    g()
class val C is Foo
  new val create() => None
  fun mk(): U8 => 3
actor Main
  new create(env: Env) => env.out.print(C.f())
