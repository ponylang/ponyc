// T.bar calls foo() with no argument, and C overrides foo with a different
// default. C's copy of bar must use C's default; expects class.
trait T
  fun foo(x: String = "trait"): String
  fun bar(): String => foo()
class C is T
  new create() => None
  fun foo(x: String = "class"): String => x
actor Main
  new create(env: Env) => env.out.print(C.bar())
