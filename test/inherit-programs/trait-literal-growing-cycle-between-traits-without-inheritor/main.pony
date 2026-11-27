// U's literal provides V[Array[A] val] and V's provides U[Array[A] val], so
// the type arguments grow around the cycle. Nothing provides T, U or V; the
// program must still compile and print nothing.
trait T[A: Any val]
  fun n(): String => "T"
  fun m(): U[Array[A] val] val => object val is U[Array[A] val] end
trait U[A: Any val]
  fun n(): String => "U"
  fun k(): V[Array[A] val] val => object val is V[Array[A] val] end
trait V[A: Any val]
  fun n(): String => "V"
  fun j(): U[Array[A] val] val => object val is U[Array[A] val] end
actor Main
  new create(env: Env) => None
