// T[A].m returns a literal of T[(A & Any val)]. A already satisfies Any val, so
// calling m three times from T[String] keeps the type argument String and the
// program compiles. Expects "n".
trait T[A: Any val]
  fun n(): String => "n"
  fun m(): T[(A & Any val)] val => object val is T[(A & Any val)] end
class C is T[String]
  new create() => None
actor Main
  new create(env: Env) => env.out.print(C.m().m().m().n())
