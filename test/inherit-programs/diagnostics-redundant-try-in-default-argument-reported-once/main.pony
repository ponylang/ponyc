// C's g() has a default argument wrapped in a try that can't error, and the
// inherited h() calls g() using that default. Expects "try expression never
// results in an error" once, at C's default argument, not again for h().
trait T
  fun g(x: U8 = 1): U8
  fun h(): U8 => g()
class C is T
  new create() => None
  fun g(x: U8 = try 3 else 4 end): U8 => x
actor Main
  new create(env: Env) => env.out.print(C.h().string())
