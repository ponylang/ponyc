// C overrides mk() to return U8, so in C's copy of f() the match else cannot
// run. It must not be reported as an error; expects 7.
trait Foo
  fun mk(): (U8 | None)
  fun f(): String =>
    match mk()
    | let u: U8 => u.string()
    else
      "none"
    end
class C is Foo
  new create() => None
  fun mk(): U8 => 7
actor Main
  new create(env: Env) => env.out.print(C.f())
