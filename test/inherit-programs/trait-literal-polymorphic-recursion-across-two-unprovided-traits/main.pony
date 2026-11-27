// T[A].m returns an object literal of U[Array[A]], and U[A].k returns one of
// U[Array[A]]. No type provides T or U; the program must compile and print
// nothing.
trait T[A: Any val]
  fun m(): U[Array[A] val] val => object val is U[Array[A] val] end
trait U[A: Any val]
  fun n(): String => "U"
  fun k(): U[Array[A] val] val => object val is U[Array[A] val] end
actor Main
  new create(env: Env) => None
