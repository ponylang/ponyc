// T.m's object literal holds a lambda that uses c.List through the file's
// alias for collections. Calling m twice from an object literal of T[U8] must
// resolve c.List and print n1. Everything is in one file.
use c = "collections"
trait T[A: Any val]
  fun n(): String => "n"
  fun m(): T[Array[A] val] val =>
    object val is T[Array[A] val]
      fun n(): String =>
        let f = {(): USize => c.List[USize].>push(4).size() }
        "n" + f().string()
    end
primitive K
  fun mk(): T[U8] val => object val is T[U8] end
actor Main
  new create(env: Env) => env.out.print(K.mk().m().m().n())
