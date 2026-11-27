// C's mk() returns String, so in C's copy of f() the if yields a String or
// the literal 1, and 1 has no type it can take. Expects "could not infer
// literal type" and "cannot infer type of x", each with a note naming method
// 'f' inherited by C from Foo.
trait Foo
  fun mk(): (U8 | String)
  fun f(c: Bool): String =>
    let x = if c then mk() else 1 end
    x.string()
class C is Foo
  new create() => None
  fun mk(): String => "s"
actor Main
  new create(env: Env) => env.out.print(C.f(false))
