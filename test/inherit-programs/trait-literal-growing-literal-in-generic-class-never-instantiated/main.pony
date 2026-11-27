// T[A].m() returns a growing T[Array[A] val] literal, and the generic class
// G[B] provides T[B] but is never used. The program must compile and print
// "ok".
trait T[A: Any val]
  fun n(): String => "n"
  fun m(): T[Array[A] val] val => object val is T[Array[A] val] end
class G[B: Any val] is T[B]
actor Main
  new create(env: Env) => env.out.print("ok")
