// C overrides mk() to return U8, so in C's copy of f() the cast to String
// cannot succeed. It must not be reported as an error; expects notstr.
trait Foo
  fun mk(): (U8 | String)
  fun f(): String =>
    try (mk() as String) else "notstr" end
class C is Foo
  new create() => None
  fun mk(): U8 => 3
actor Main
  new create(env: Env) => env.out.print(C.f())
