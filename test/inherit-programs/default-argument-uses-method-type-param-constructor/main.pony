// The default for x is B.create(), where B is m's own type parameter. Calling
// C.m[K]() must build a K for the default; expects k.
interface Mk
  new create()
  fun s(): String
class K
  new create() => None
  fun s(): String => "k"
trait T
  fun m[B: Mk](x: B = B.create()): String => x.s()
class C is T
  new create() => None
actor Main
  new create(env: Env) => env.out.print(C.m[K]())
