// C is not Stringable, so in C's copy of f() the Stringable case on this
// cannot match. It must not be reported as an error; expects plain.
trait Foo
  fun f(): String =>
    match this
    | let s: Stringable box => s.string()
    else "plain" end
class C is Foo
  new create() => None
actor Main
  new create(env: Env) => env.out.print(C.f())
