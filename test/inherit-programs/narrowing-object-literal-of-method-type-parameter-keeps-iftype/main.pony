// A generic method returns an object literal of Tr[Y], whose g uses iftype on
// its type parameter. Expects "9" for U32 and "None" for None: each literal
// keeps the narrowing for its own type argument (#6735).
trait Tr[X: Any val]
  fun g(x: X): String =>
    iftype X <: Stringable val then
      x.string()
    else "no" end

primitive P
  fun make[Y: Any val](): Tr[Y] val => object is Tr[Y] end

actor Main
  new create(env: Env) =>
    env.out.print(P.make[U32]().g(9))
    env.out.print(P.make[None]().g(None))
