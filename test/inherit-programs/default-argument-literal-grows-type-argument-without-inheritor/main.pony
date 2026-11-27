// U.h's default is a literal of T[B], and T.m calls h on a
// U[Array[A] val]. Nothing provides T; the program must still compile and
// print x.
trait U[B: Any val]
  fun h(x: Any val = object val is T[B] end): String => "h"

trait T[A: Any val]
  fun m(u: U[Array[A] val] val): String => u.h()
  fun n(): String => "T"

primitive UP[B: Any val] is U[B]

class val CC
  new val create() => None

actor Main
  new create(env: Env) =>
    env.out.print("x")
