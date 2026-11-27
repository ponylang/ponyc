// C overrides g with a default argument whose String pattern can never match
// a U8. T.h, inherited by C, calls g() and uses that default. The error must
// be reported once, at C's default argument.
trait T
  fun g(x: U8 = 1): U8
  fun h(): U8 => g()
class C is T
  new create() => None
  fun g(x: U8 = match U8(3) | let s: String => 9 else 4 end): U8 => x
actor Main
  new create(env: Env) => env.out.print(C.h().string())
