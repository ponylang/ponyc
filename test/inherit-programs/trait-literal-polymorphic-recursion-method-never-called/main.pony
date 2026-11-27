// T[A].m returns an object literal of T[Array[A]]. Main makes an object
// literal of T[U8] and never calls m, only n(); the program must compile and
// print "n".
trait T[A: Any val]
  fun n(): String => "n"
  fun m(): T[Array[A] val] val => object val is T[Array[A] val] end
actor Main
  new create(env: Env) =>
    let o = object val is T[U8] end
    env.out.print(o.n())
