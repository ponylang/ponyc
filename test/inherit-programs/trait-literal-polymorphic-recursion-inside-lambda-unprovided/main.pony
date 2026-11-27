// T[A].m returns a lambda that returns an object literal of T[Array[A]]. No
// type provides T; the program must compile and print "ok".
trait T[A: Any val]
  fun n(): String => "n"
  fun m(): {(): T[Array[A] val] val} val => {(): T[Array[A] val] val => object val is T[Array[A] val] end}
actor Main
  new create(env: Env) => env.out.print("ok")
