// T.m captures a lambda that uses Array[A] in a literal of T[Array[A] val].
// Three levels deep from a primitive's literal of T[U8], expects "1".
trait T[A: Any val]
  fun n(): String => "n"
  fun m(): T[Array[A] val] val =>
    let f = {(): USize => Array[A].size() + 1}
    object val is T[Array[A] val]
      fun n(): String => f().string()
    end
primitive K
  fun mk(): T[U8] val => object val is T[U8] end
actor Main
  new create(env: Env) => env.out.print(K.mk().m().m().m().n())
