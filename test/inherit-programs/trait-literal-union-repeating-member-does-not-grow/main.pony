// T[A].m returns a literal of T[(A | U8)]. Calling m three times from
// T[String] only repeats U8 in the union, so the type argument stays
// (String | U8) and the program compiles. Expects "n".
trait T[A: Any val]
  fun n(): String => "n"
  fun m(): T[(A | U8)] val => object val is T[(A | U8)] end
class C is T[String]
  new create() => None
actor Main
  new create(env: Env) => env.out.print(C.m().m().m().n())
