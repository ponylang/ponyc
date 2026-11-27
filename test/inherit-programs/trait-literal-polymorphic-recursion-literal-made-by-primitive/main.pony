// T[A].m returns an object literal of T[Array[A]]. Primitive K returns an
// object literal of T[U8]; calling only n() on it must compile and print
// "n".
trait T[A: Any val]
  fun n(): String => "n"
  fun m(): T[Array[A] val] val => object val is T[Array[A] val] end
primitive K
  fun mk(): T[U8] val => object val is T[U8] end
actor Main
  new create(env: Env) => env.out.print(K.mk().n())
