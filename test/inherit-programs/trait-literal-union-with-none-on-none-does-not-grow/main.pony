// T[A].m returns a literal of T[(A | None)]. Calling m three times from
// T[None] keeps the type argument None, so the program compiles. Expects "n".
trait T[A: Any val]
  fun n(): String => "n"
  fun m(): T[(A | None)] val => object val is T[(A | None)] end
class C is T[None]
  new create() => None
actor Main
  new create(env: Env) => env.out.print(C.m().m().m().n())
