// C overrides g() without ?, so in C's copy of f() the g()? call cannot raise.
// It must not be reported as an error; expects 1.
trait T
  fun g(): U8 ?
  fun f(): U8 ? => g()?
class C is T
  new create() => None
  fun g(): U8 => 1
actor Main
  new create(env: Env) => try env.out.print(C.f()?.string()) end
