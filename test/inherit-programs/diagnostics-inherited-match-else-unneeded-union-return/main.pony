// C overrides foo() to return U8, so in C's copy of bar() the match else
// cannot run. It must not be reported as an error; expects u8 3.
trait T
  fun foo(): (U8 | None)
  fun bar(): String =>
    match foo() | let u: U8 => "u8 " + u.string() else "none" end
class C is T
  new create() => None
  fun foo(): U8 => 3
actor Main
  new create(env: Env) => env.out.print(C.bar())
