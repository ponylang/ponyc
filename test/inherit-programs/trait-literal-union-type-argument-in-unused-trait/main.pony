// T.m returns an object literal of T[(A | U8)]. Nothing inherits T or calls
// m. The program must compile and print x.
trait T[A: Any val]
  fun n(): String => "n"
  fun m(): T[(A | U8)] val => object val is T[(A | U8)] end
actor Main
  new create(env: Env) => env.out.print("x")
