// U.h's default is a literal of T[B], and T.m calls h on a
// U[Array[A] val], so the type arguments grow through the default. C.m
// must compile and print hT.
trait U[B: Any val]
  fun h(x: Any val = object val is T[B] end): String => "h"

trait T[A: Any val]
  fun m(u: U[Array[A] val] val): String => u.h()
  fun n(): String => "T"

primitive UP[B: Any val] is U[B]

class val C is T[U8]
  new val create() => None

actor Main
  new create(env: Env) =>
    env.out.print(C.m(UP[Array[U8] val]) + C.n())
