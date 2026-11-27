// T[A].m() returns a growing T[Array[A] val] literal, and C is T[U8].
// C.m().m().name() must compile and run: each literal's class is built with
// the A of the type it was reached through.
trait T[A: Any val]
  fun name(): String => "T"
  fun m(): T[Array[A] val] val => object val is T[Array[A] val] end
class C is T[U8]
actor Main
  new create(env: Env) => env.out.print(C.m().m().name())
