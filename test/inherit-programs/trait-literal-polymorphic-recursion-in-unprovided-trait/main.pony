// T[A].m returns an object literal of T[Array[A]], and no type provides T.
// The program must compile and print nothing.
trait T[A: Any val]
  fun n(): String => "n"
  fun m(): T[Array[A] val] val => object val is T[Array[A] val] end
actor Main
  new create(env: Env) => None
